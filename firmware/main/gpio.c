/**
 * @file gpio.cpp
 * @author Dorian Benech
 * @brief
 * @version 1.0
 * @date 2023-10-11
 *
 * @copyright Copyright (c) 2023 GammaTroniques
 *
 */

/*==============================================================================
 Local Include
===============================================================================*/
#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/event_groups.h"
#include "driver/gpio.h"
#include "esp_adc/adc_oneshot.h"
#include "esp_adc/adc_cali.h"
#include "esp_adc/adc_cali_scheme.h"
#include "esp_log.h"
#include "soc/rtc.h"
#include "esp_pm.h"
#include <led_strip.h>

#include "gpio.h"
#include "config.h"
#include "wifi.h"
#include "main.h"
#include "zigbee.h"
#include "tuya.h"
#include "ota.h"

/*==============================================================================
 Local Define
===============================================================================*/
#define TAG "GPIO"
#define PATTERN_SIZE 3

/*==============================================================================
 Local Macro
===============================================================================*/

/*==============================================================================
 Local Type
===============================================================================*/
typedef enum
{
    NO_FLASH,
    PAIRING_FLASH,
    WEB_FLASH = 0x0008FF,
    ZIGBEE_FLASH = 0xFF0000,
    TUYA_FLASH = 0xFA650F,
} led_state_t;

typedef struct ledPattern_t
{
    uint32_t color;
    uint32_t tOn;
    uint32_t tOff;
} ledPattern_t;

/*==============================================================================
 Local Function Declaration
===============================================================================*/
static void gpio_set_led_color(uint32_t color);
static bool gpio_adc_calibration_init(adc_unit_t unit, adc_channel_t channel, adc_atten_t atten, adc_cali_handle_t *out_handle);
static void gpio_led_pattern_task(void *pvParameters);
static void gpio_set_led_rgb(uint32_t color, uint32_t brightness);
static void gpio_init_adc(adc_unit_t adc_unit, adc_oneshot_unit_handle_t *out_handle);
static void gpio_init_adc_cali(adc_oneshot_unit_handle_t adc_handle, adc_channel_t adc_channel, adc_cali_handle_t *out_adc_cali_handle, char *name);

/*==============================================================================
Public Variable
===============================================================================*/
TaskHandle_t gpip_led_ota_task_handle = NULL;
TaskHandle_t gpio_led_pairing_task_handle = NULL;
/*==============================================================================
 Local Variable
===============================================================================*/
static adc_oneshot_unit_handle_t adc1_handle = NULL;
static adc_cali_handle_t adc_usb_cali_handle = NULL;
static adc_cali_handle_t adc_capa_cali_handle = NULL;

static led_strip_handle_t led;

// clang-format off
static const ledPattern_t ledPattern[][PATTERN_SIZE] = {
    {{0x0008FF,      100, 400}                                                            }, // WIFI_CONNECTING // TODO: remove 
    {{0xFF8000,      200, 100}                                                            }, // WIFI_RETRY, new try // TODO: remove 
    {{0xFF0000,      200, 100},                                                           }, // WIFI_FAILED
    {{0x5EFF00,      500, 100}                                                            }, // LINKY_OK // TODO: remove 
    {{0xFF00F2,     1000, 100}                                                            }, // LINKY_ERR
    {{0x00FF00,      200, 500},  {0x00FF00,      200, 500}                              }, // SEND_OK
    {{0xFF0000,      200, 1000}, {0xFF0000,      200, 1000}                             }, // SEND_ERR
    {{0xFF0000,       50, 100},  {0xFF0000,       50, 100}, {0xFF0000,       50, 100 }}, // NO_CONFIG // TODO: remove 
    {{0xE5FF00,       50,   0}                                                            }, // START
    {{0x8803FC,       100,  400}                                                          }, // PAIRING
};

static uint8_t pattern_in_progress = 0;
// clang-format on

/*==============================================================================
Function Implementation
===============================================================================*/

void gpio_init_pins()
{
    gpio_set_direction(LED_EN, GPIO_MODE_OUTPUT);
    // gpio_set_direction(LED_DATA, GPIO_MODE_OUTPUT);
    gpio_set_level(LED_EN, 1);

    /* LED strip initialization with the GPIO and pixels number*/
    led_strip_config_t strip_config = {
        .strip_gpio_num = LED_DATA,               // The GPIO that connected to the LED strip's data line
        .max_leds = 1,                            // The number of LEDs in the strip,
        .led_pixel_format = LED_PIXEL_FORMAT_GRB, // Pixel format of your LED strip
        .led_model = LED_MODEL_WS2812,            // LED strip model
        .flags = {
            .invert_out = false, // whether to invert the output signal (useful when your hardware has a level inverter)
        },
    };

    led_strip_rmt_config_t rmt_config = {
        .clk_src = RMT_CLK_SRC_DEFAULT,    // different clock source can lead to different power consumption
        .resolution_hz = 10 * 1000 * 1000, // 10MHz
        .mem_block_symbols = 0,
        .flags = {
            .with_dma = false, // whether to enable the DMA feature
        },
    };

    ESP_ERROR_CHECK(led_strip_new_rmt_device(&strip_config, &rmt_config, &led));

    led_strip_clear(led);
    led_strip_refresh(led);

    gpio_init_adc(ADC_UNIT_1, &adc1_handle);
    gpio_init_adc_cali(adc1_handle, V_USB_PIN, &adc_usb_cali_handle, "VUSB");
    gpio_init_adc_cali(adc1_handle, V_CONDO_PIN, &adc_capa_cali_handle, "VCondo");

    // gpio_set_direction(LED_DATA, GPIO_MODE_INPUT); // HIGH-Z
    ESP_LOGI(TAG, "VCondo: %fV", gpio_get_vcondo());
    ESP_LOGI(TAG, "VUSB: %fV", gpio_get_vusb());
}

static void gpio_init_adc(adc_unit_t adc_unit, adc_oneshot_unit_handle_t *out_handle)
{
    esp_err_t ret = ESP_OK;

    adc_oneshot_unit_init_cfg_t vusb_init_config = {
        .unit_id = adc_unit,
    };
    ret = adc_oneshot_new_unit(&vusb_init_config, out_handle);
    if (ret != ESP_OK)
    {
        ESP_LOGE(TAG, "Failed to init ADC: %s", esp_err_to_name(ret));
    }
}

static void gpio_init_adc_cali(adc_oneshot_unit_handle_t adc_handle, adc_channel_t adc_channel, adc_cali_handle_t *out_adc_cali_handle, char *name)
{
    esp_err_t ret = ESP_OK;
    adc_oneshot_chan_cfg_t config = {
        .atten = ADC_ATTEN_DB_11,
        .bitwidth = ADC_BITWIDTH_DEFAULT,
    };

    ret = adc_oneshot_config_channel(adc_handle, adc_channel, &config);
    if (ret != ESP_OK)
    {
        ESP_LOGE(TAG, "Failed to init %s ADC: %s", name, esp_err_to_name(ret));
    }

    bool cal = gpio_adc_calibration_init(ADC_UNIT_1, adc_channel, ADC_ATTEN_DB_11, out_adc_cali_handle);
    if (!cal)
    {
        ESP_LOGW(TAG, "Failed to init USB ADC calibration");
        out_adc_cali_handle = NULL;
    }
}
/**
 * @brief
 *
 * @param color
 * @param brightness in per thousand
 */
static void gpio_set_led_rgb(uint32_t color, uint32_t brightness)
{
    uint32_t r = (color >> 16) & 0xFF;
    uint32_t g = (color >> 8) & 0xFF;
    uint32_t b = (color >> 0) & 0xFF;

    // set brightness
    r = (r * brightness) / 1000;
    g = (g * brightness) / 1000;
    b = (b * brightness) / 1000;

    ESP_LOGD(TAG, "r: %ld, g: %ld, b: %ld, brightness: %ld", r, g, b, brightness);
    led_strip_set_pixel(led, 0, r, g, b);
    led_strip_refresh(led);
}

static void gpio_set_led_color(uint32_t color)
{
    if (color == 0)
    {
        led_strip_clear(led);
        led_strip_refresh(led);
        vTaskDelay(1);
        gpio_set_level(LED_EN, 0);
        // gpio_set_direction(LED_DATA, GPIO_MODE_INPUT); // HIGH-Z
        return;
    }
    gpio_set_level(LED_EN, 1);
    gpio_set_led_rgb(color, 50); // 5% brightness
    // gpio_set_direction(LED_DATA, GPIO_MODE_OUTPUT);
}

static bool gpio_adc_calibration_init(adc_unit_t unit, adc_channel_t channel, adc_atten_t atten, adc_cali_handle_t *out_handle)
{
    adc_cali_handle_t handle = NULL;
    esp_err_t ret = ESP_FAIL;
    bool calibrated = false;

#if ADC_CALI_SCHEME_CURVE_FITTING_SUPPORTED
    if (!calibrated)
    {
        // ESP_LOGI(TAG, "calibration scheme version is %s", "Curve Fitting");
        adc_cali_curve_fitting_config_t cali_config = {
            .unit_id = unit,
            .chan = channel,
            .atten = atten,
            .bitwidth = ADC_BITWIDTH_DEFAULT,
        };
        ret = adc_cali_create_scheme_curve_fitting(&cali_config, &handle);
        if (ret == ESP_OK)
        {
            calibrated = true;
        }
    }
#endif

#if ADC_CALI_SCHEME_LINE_FITTING_SUPPORTED
    if (!calibrated)
    {
        ESP_LOGI(TAG, "calibration scheme version is %s", "Line Fitting");
        adc_cali_line_fitting_config_t cali_config = {
            .unit_id = unit,
            .atten = atten,
            .bitwidth = ADC_BITWIDTH_DEFAULT,
        };
        ret = adc_cali_create_scheme_line_fitting(&cali_config, &handle);
        if (ret == ESP_OK)
        {
            calibrated = true;
        }
    }
#endif

    *out_handle = handle;
    if (ret == ESP_OK)
    {
        // ESP_LOGI(TAG, "Calibration Success");
    }
    else if (ret == ESP_ERR_NOT_SUPPORTED || !calibrated)
    {
        ESP_LOGW(TAG, "eFuse not burnt, skip software calibration");
    }
    else
    {
        ESP_LOGE(TAG, "Invalid arg or no memory: %s, cal: %d", esp_err_to_name(ret), calibrated);
    }
    return calibrated;
}

float gpio_get_vcondo()
{
    esp_err_t ret = ESP_OK;
    int raw = 0;
    ret = adc_oneshot_read(adc1_handle, V_CONDO_PIN, &raw);
    if (ret != ESP_OK)
    {
        ESP_LOGE(TAG, "Failed to read VCondo: %s", esp_err_to_name(ret));
        return 0.0;
    }

    int vADC = 0;
    ret = adc_cali_raw_to_voltage(adc_capa_cali_handle, raw, &vADC);
    if (ret == ESP_ERR_INVALID_STATE)
    {
        ESP_LOGE(TAG, "Failed to read VCondo: %s", esp_err_to_name(ret));
        return (float)(raw * 3.3 / 4095);
    }
    else if (ret != ESP_OK)
    {
        ESP_LOGE(TAG, "Failed to calibrate VCondo: %s", esp_err_to_name(ret));
        return 0.0;
    }

    uint32_t vCondo = (vADC * 280) / 180;
    return (float)vCondo / 1000;
}

float gpio_get_vusb()
{
    esp_err_t ret = ESP_OK;
    int raw = 0;
    ret = adc_oneshot_read(adc1_handle, V_USB_PIN, &raw);
    if (ret != ESP_OK)
    {
        ESP_LOGE(TAG, "Failed to read VUSB: %s", esp_err_to_name(ret));
        return 0.0;
    }
    int vADC = 0;
    ret = adc_cali_raw_to_voltage(adc_usb_cali_handle, raw, &vADC);
    if (ret == ESP_ERR_INVALID_STATE)
    {
        ESP_LOGW(TAG, "VUSB not calibrated: %s", esp_err_to_name(ret));
        return (float)(raw * 3.3 / 4095);
    }
    else if (ret != ESP_OK)
    {
        ESP_LOGE(TAG, "Failed to calibrate VUSB: %s", esp_err_to_name(ret));
        return 0.0;
    }

    uint32_t vUSB = (vADC * 280) / 180;
    int vusb_level = gpio_get_level(V_USB_PIN);
    ESP_LOGW(TAG, "vUSB: %ld, ret: %d", vUSB, vusb_level);

    // return (float)vUSB / 1000;
    if (vusb_level)
    {
        return 5.0;
    }
    else
    {
        return 0.0;
    }
}

void gpio_pairing_button_task(void *pvParameters)
{
    uint32_t startPushTime = 0;
    uint8_t lastState = 1;
    uint32_t pushTime = MILLIS - startPushTime;
    led_state_t ledState = NO_FLASH;
    uint8_t pairingState = 0;
    while (1)
    {
        if (gpio_get_level(PAIRING_PIN) == 0) // if button is pushed
        {
            if (lastState == 1)
            {
                ESP_LOGI(TAG, "Start pushing");
                suspendTask(tuyaTaskHandle);
                suspendTask(noConfigLedTaskHandle);
                lastState = 0;
                ledState = NO_FLASH;
                startPushTime = MILLIS;
            }
            pushTime = MILLIS - startPushTime;
            if (pushTime > 4000)
            {
                // Color Wheel
                ESP_LOGI(TAG, "pushTime: %lu, %%: %lu", pushTime, pushTime % 1000);
                if (pushTime % 1000 == 0)
                {
                    switch (ledState)
                    {
                    case NO_FLASH:
                        ledState = WEB_FLASH;
                        break;
                    case PAIRING_FLASH:
                        ledState = WEB_FLASH;
                        break;
                    case WEB_FLASH:
                        ledState = ZIGBEE_FLASH;
                        break;
                    case ZIGBEE_FLASH:
                        ledState = TUYA_FLASH;
                        break;
                    case TUYA_FLASH:
                        ledState = WEB_FLASH;
                        break;
                    default:
                        break;
                    }
                    ESP_LOGI(TAG, "------------------------LED state: %d", ledState);
                    gpio_set_led_color(ledState);
                }
            }
            else if (pushTime > 2000)
            {
                // Announce pairing mode
                if (ledState == NO_FLASH)
                {
                    ledState = PAIRING_FLASH; // flash pairing led
                    suspendTask(noConfigLedTaskHandle);
                    gpio_start_led_pattern(PATTERN_PAIRING);
                }
            }
            else
            {
                ledState = NO_FLASH;
            }
        }
        else // if button is not pushed
        {
            if (lastState == 0) // button was released
            {
                ESP_LOGI(TAG, "Button pushed for %lu ms", pushTime);
                if (pushTime > 5000)
                {
                    if (noConfigLedTaskHandle != NULL)
                    {
                        vTaskDelete(noConfigLedTaskHandle);
                        noConfigLedTaskHandle = NULL;
                    }
                    ESP_LOGI(TAG, "Changing mode");
                    switch (ledState)
                    {
                    case WEB_FLASH:
                        config_values.mode = MODE_WEB;
                        break;
                    case ZIGBEE_FLASH:
                        config_values.mode = MODE_ZIGBEE;
                        break;
                    case TUYA_FLASH:
                        config_values.mode = MODE_TUYA;
                        break;
                    default:
                        break;
                    }
                    config_write();
                    vTaskDelay(2000 / portTICK_PERIOD_MS);
                    esp_restart();
                }
                else if (pushTime > 2000 && pushTime <= 4000)
                {
                    ESP_LOGI(TAG, "Pairing mode");
                    xTaskCreate(gpio_led_task_pairing, "gpio_led_task_pairing", 2048, NULL, 5, &gpio_led_pairing_task_handle);
                    if (pairingState)
                    {
                        // already in pairing mode
                        esp_restart();
                    }
                    switch (config_values.mode)
                    {
                    case MODE_WEB:
                    case MODE_MQTT:
                    case MODE_MQTT_HA:
                        ESP_LOGI(TAG, "Web pairing");
                        pairingState = 1;
                        suspendTask(fetchLinkyDataTaskHandle);
                        if (wifi_state == WIFI_CONNECTED)
                        {
                            wifi_disconnect();
                            vTaskDelay(1000 / portTICK_PERIOD_MS);
                        }
                        ESP_LOGI(TAG, "Starting captive portal");
                        wifi_start_captive_portal();
                        break;
                    case MODE_TUYA:
                        pairingState = 1;
                        ESP_LOGI(TAG, "Tuya pairing");
                        xTaskCreate(tuya_pairing_task, "tuya_pairing_task", 8 * 1024, NULL, 5, NULL);
                        break;
                    case MODE_ZIGBEE:
                        pairingState = 1;
                        ESP_LOGI(TAG, "Zigbee pairing TODO");
                        // start_zigbee_pairing();
                        esp_zb_factory_reset();
                        break;
                    default:
                        ESP_LOGI(TAG, "No pairing mode");
                        break;
                    }
                }
                else
                {
                    if (ota_state == OTA_AVAILABLE && gpio_get_vusb() > 3)
                    {
                        ESP_LOGI(TAG, "OTA available, starting update");
                        suspendTask(fetchLinkyDataTaskHandle);
                        resumeTask(noConfigLedTaskHandle);
                        resumeTask(tuyaTaskHandle);
                        ota_state = OTA_INSTALLING;
                        vTaskDelay(500 / portTICK_PERIOD_MS); // wait for led task to update
                        xTaskCreate(ota_perform_task, "ota_perform_task", 16 * 1024, NULL, 5, NULL);
                    }
                    else
                    {
                        gpio_boot_led_pattern();
                        vTaskDelay(500 / portTICK_PERIOD_MS);
                        resumeTask(noConfigLedTaskHandle);
                        resumeTask(tuyaTaskHandle);
                        ESP_LOGI(TAG, "No action");
                    }
                }
                lastState = 1;
                ledState = NO_FLASH;
            }
        }
        vTaskDelay(50 / portTICK_PERIOD_MS);
    }
}

static void gpio_led_pattern_task(void *pvParameters)
{
    uint8_t id = *(uint8_t *)pvParameters;
    pattern_in_progress = 1;
    for (int i = 0; i < PATTERN_SIZE; i++)
    {
        gpio_set_led_color(ledPattern[id][i].color);
        vTaskDelay(ledPattern[id][i].tOn / portTICK_PERIOD_MS);
        gpio_set_led_color(0);
        vTaskDelay(ledPattern[id][i].tOff / portTICK_PERIOD_MS);
    }
    pattern_in_progress = 0;
    vTaskDelete(NULL); // Delete this task
}

void gpio_start_led_pattern(uint8_t pattern)
{
    static TaskHandle_t ledPatternTaskHandle = NULL;
    static uint8_t lastPattern;
    lastPattern = pattern;
    xTaskCreate(gpio_led_pattern_task, "gpio_led_pattern_task", 2048, &lastPattern, 5, &ledPatternTaskHandle);
}

void gpio_boot_led_pattern()
{
    switch (config_values.mode)
    {
    case MODE_WEB:
    case MODE_MQTT:
    case MODE_MQTT_HA:
        gpio_set_led_color(WEB_FLASH);
        break;

    case MODE_TUYA:
        gpio_set_led_color(TUYA_FLASH);
        break;
    case MODE_ZIGBEE:
        gpio_set_led_color(ZIGBEE_FLASH);
        break;
    default:
        break;
    }
    vTaskDelay(100 / portTICK_PERIOD_MS);
    gpio_set_led_color(0);
}

void gpio_led_task_no_config(void *pvParameters)
{
    pattern_in_progress = 1;
    while (config_verify())
    {
        vTaskDelay(5000 / portTICK_PERIOD_MS);
        for (int i = 0; i < 3; i++)
        {
            gpio_set_led_color(0xFF0000);
            vTaskDelay(50 / portTICK_PERIOD_MS);
            gpio_set_led_color(0);
            vTaskDelay(100 / portTICK_PERIOD_MS);
        }
    }
    pattern_in_progress = 0;
    vTaskDelete(NULL); // Delete this task
}

void gpio_led_task_wifi_connecting(void *pvParameters)
{
    ESP_LOGI(TAG, "Starting wifi led task");
    uint32_t timout = MILLIS + WIFI_CONNECT_TIMEOUT;
    pattern_in_progress = 1;

    ESP_LOGI(TAG, "wifi_state: %d", wifi_state);
    while (wifi_state == WIFI_CONNECTING && MILLIS < timout)
    {
        gpio_set_led_color(WEB_FLASH);
        vTaskDelay(100 / portTICK_PERIOD_MS);
        gpio_set_led_color(0);
        vTaskDelay(900 / portTICK_PERIOD_MS);
    }
    pattern_in_progress = 0;
    vTaskDelete(NULL); // Delete this task
}

void gpio_led_task_linky_reading(void *pvParameters)
{
    pattern_in_progress = 1;
    while (linky_reading)
    {
        gpio_set_led_color(0xFF8000);
        vTaskDelay(100 / portTICK_PERIOD_MS);
        gpio_set_led_color(0);
        vTaskDelay(900 / portTICK_PERIOD_MS);
    }
    pattern_in_progress = 0;
    vTaskDelete(NULL); // Delete this task
}

void gpio_led_task_sending(void *pvParameters)
{
    pattern_in_progress = 1;
    while (wifi_sending)
    {
        gpio_set_led_color(0xc300ff);
        vTaskDelay(100 / portTICK_PERIOD_MS);
        gpio_set_led_color(0);
        vTaskDelay(900 / portTICK_PERIOD_MS);
    }
    pattern_in_progress = 0;
    vTaskDelete(NULL); // Delete this task
}

void gpio_led_task_pairing(void *pvParameters)
{
    while (1)
    {
        gpio_set_led_color(0x8803FC);
        vTaskDelay(100 / portTICK_PERIOD_MS);
        gpio_set_led_color(0);
        vTaskDelay(900 / portTICK_PERIOD_MS);
    }
    vTaskDelete(NULL); // Delete this task
}

void gpio_led_task_ota(void *pvParameters)
{
    ESP_LOGI(TAG, "Starting OTA led task");
    int32_t brightness = 0;
    uint32_t color = 0;
    typedef enum
    {
        VUSB_TOO_LOW,
        WAIT_USB,
        OFF,
        RISING,
        ON,
        FALLING,
    } led_state_t;

    led_state_t state = OFF;
    uint8_t last_pattern_in_progress = 0;
    uint16_t animation_delay = 10;
    uint16_t increment = 1;
    uint16_t on_delay = 1000;
    uint16_t off_delay = 5000;
    while (1)
    {
        gpio_set_level(LED_EN, 1);
        // ESP_LOGW(TAG, "OTA state: %d, state: %d, shift: %ld, brightness: %ld", ota_state, state, color, brightness);

        switch (ota_state)
        {
        case OTA_AVAILABLE:
            color = 0x0000FF;
            break;
        case OTA_INSTALLING:
            color = 0xFFFF00;
            // force led
            pattern_in_progress = 0;
            last_pattern_in_progress = 0;
            increment = 5;
            on_delay = 1000;
            off_delay = 1000;
            break;
        case OTA_OK:
            color = 0x00FF00;
            pattern_in_progress = 0;
            last_pattern_in_progress = 0;
            increment = 10;
            on_delay = 500;
            off_delay = 500;
            break;
        case OTA_ERROR:
            color = 0xFF0000;
            pattern_in_progress = 0;
            last_pattern_in_progress = 0;
            increment = 10;
            on_delay = 500;
            off_delay = 500;
            break;
        default:
            break;
        }

        if (pattern_in_progress) // if another pattern is in progress, wait: not important at the moment
        {
            last_pattern_in_progress = 1;
            vTaskDelay(100 / portTICK_PERIOD_MS);
            continue;
        }

        if (last_pattern_in_progress)
        {
            last_pattern_in_progress = 0;
            state = OFF;
            vTaskDelay(10000 / portTICK_PERIOD_MS);
        }

        switch (state)
        {
        case VUSB_TOO_LOW:
            brightness = 0;
            state = WAIT_USB;
            gpio_set_led_rgb(color, brightness);
            vTaskDelay(1000 / portTICK_PERIOD_MS);
            break;
        case WAIT_USB:
            if (gpio_get_vusb() > 3)
            {
                state = OFF;
            }
            vTaskDelay(1000 / portTICK_PERIOD_MS);
            break;

        case OFF:
            brightness = 0;
            state = RISING;
            gpio_set_led_rgb(color, brightness);
            vTaskDelay(off_delay / portTICK_PERIOD_MS);

            break;
        case RISING:
            if (brightness >= 100)
            {
                state = ON;
            }
            else
            {
                brightness += increment;
            }
            gpio_set_led_rgb(color, brightness);
            vTaskDelay(animation_delay / portTICK_PERIOD_MS);
            break;
        case ON:
            vTaskDelay(on_delay / portTICK_PERIOD_MS);
            state = FALLING;
            break;
        case FALLING:
            if (brightness <= 0)
            {
                state = OFF;
            }
            else
            {
                brightness -= increment;
            }
            gpio_set_led_rgb(color, brightness);
            vTaskDelay(animation_delay / portTICK_PERIOD_MS);

            break;
        default:
            vTaskDelay(50 / portTICK_PERIOD_MS);
            break;
        }
        if (gpio_get_vusb() < 3.0 && state != WAIT_USB)
        {
            ESP_LOGE(TAG, "VUSB too low: dont animate");
            state = VUSB_TOO_LOW;
        }
    }
    vTaskDelete(NULL); // Delete this task
}