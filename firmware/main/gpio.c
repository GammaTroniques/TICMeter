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
#include "esp_sleep.h"

#include "gpio.h"
#include "config.h"
#include "wifi.h"
#include "main.h"
#include "zigbee.h"
#include "tuya.h"
#include "ota.h"
#include "power.h"

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

const uint32_t color_mode[] = {
    [MODE_NONE] = 0x000000,
    [MODE_WEB] = 0x0008FF,
    [MODE_MQTT] = 0x8803FC,
    [MODE_MQTT_HA] = 0x8803FC,
    [MODE_ZIGBEE] = 0xFF0000,
    [MODE_MATTER] = 0xFFFFFF,
    [MODE_TUYA] = 0xFA650F,
};

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
static void gpio_pairing_isr_cb(void *arg);
static void gpio_vusb_isr_cb(void *arg);
static void gpio_vusb_task(void *pvParameter);

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

static QueueHandle_t gpio_pairing_button_isr_queue = NULL;
static QueueHandle_t power_vusb_isr_queue = NULL;
static volatile uint8_t vusb_level = 0;

static esp_pm_lock_handle_t gpio_pairing_lock = NULL;
static esp_pm_lock_handle_t gpio_vusb_lock = NULL;

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
    {{0xFF0000,       500, 500},  {0xFF0000,       500, 500}, {0xFF0000,       2000, 100}    }, // FACTORY_RESET
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

    esp_pm_lock_create(ESP_PM_NO_LIGHT_SLEEP, 0, "gpio_pairing_lock", &gpio_pairing_lock);
    esp_pm_lock_create(ESP_PM_NO_LIGHT_SLEEP, 0, "gpio_vusb_lock", &gpio_vusb_lock);

    gpio_config_t pairing_conf = {
        .pin_bit_mask = (1ULL << PAIRING_PIN),
        .mode = GPIO_MODE_INPUT,
        .pull_up_en = GPIO_PULLUP_DISABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_LOW_LEVEL, // light sleep dont support edges
    };
    gpio_config(&pairing_conf);

    gpio_config_t vusb_conf = {
        .pin_bit_mask = (1ULL << V_USB_PIN),
        .mode = GPIO_MODE_INPUT,
        .pull_up_en = GPIO_PULLUP_DISABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
    };
    gpio_config(&vusb_conf);

    vusb_level = gpio_get_level(V_USB_PIN);
    if (vusb_level)
    {
        ESP_LOGI(TAG, "USB already connected");
        esp_pm_lock_acquire(gpio_vusb_lock);
    }
    ESP_LOGI(TAG, "VUSB level: %d", vusb_level);
    vusb_conf.intr_type = vusb_level ? GPIO_INTR_LOW_LEVEL : GPIO_INTR_HIGH_LEVEL;

    gpio_install_isr_service(0);
    gpio_isr_handler_add(PAIRING_PIN, gpio_pairing_isr_cb, (void *)PAIRING_PIN);
    gpio_isr_handler_add(V_USB_PIN, gpio_vusb_isr_cb, (void *)V_USB_PIN);

    gpio_pairing_button_isr_queue = xQueueCreate(10, sizeof(uint32_t));
    power_vusb_isr_queue = xQueueCreate(1, sizeof(uint32_t));

    // gpio_set_direction(LED_DATA, GPIO_MODE_INPUT); // HIGH-Z
    ESP_LOGI(TAG, "VCondo: %fV", gpio_get_vcondo());
    ESP_LOGI(TAG, "VUSB: %fV", gpio_get_vusb());

    gpio_wakeup_enable(PAIRING_PIN, GPIO_INTR_LOW_LEVEL);
    gpio_wakeup_enable(V_USB_PIN, vusb_level ? GPIO_INTR_LOW_LEVEL : GPIO_INTR_HIGH_LEVEL);
    esp_sleep_enable_gpio_wakeup();

    xTaskCreate(gpio_pairing_button_task, "gpio_pairing_button_task", 8192, NULL, PRIORITY_PAIRING, NULL); // start push button task
    xTaskCreate(gpio_vusb_task, "gpio_vusb_task", 4 * 1024, NULL, PRIORITY_PAIRING, NULL);                 // start push button task
}

static void IRAM_ATTR gpio_pairing_isr_cb(void *arg)
{
    uint32_t gpio_num = (uint32_t)arg;
    esp_rom_printf("IT PAIR %ld\n", gpio_num);
    uint8_t level = gpio_get_level(gpio_num);
    if (level == 0) // if button is pushed
    {
        gpio_intr_disable(gpio_num);
        xQueueSendFromISR(gpio_pairing_button_isr_queue, &gpio_num, NULL);
    }
    portYIELD_FROM_ISR();
}

static void IRAM_ATTR gpio_vusb_isr_cb(void *arg)
{
    uint32_t gpio_num = (uint32_t)arg;
    uint32_t level = gpio_get_level(gpio_num);
    // esp_rom_printf("IT USB %ld lvl: %ld\n", gpio_num, level);
    gpio_wakeup_disable(gpio_num);
    gpio_intr_disable(gpio_num);
    if (level == 0)
    {
        gpio_wakeup_enable(gpio_num, GPIO_INTR_HIGH_LEVEL);
    }
    else
    {
        gpio_wakeup_enable(gpio_num, GPIO_INTR_LOW_LEVEL);
    }
    gpio_intr_enable(gpio_num);
    xQueueOverwriteFromISR(power_vusb_isr_queue, &level, NULL);
}

static void gpio_vusb_task(void *pvParameter)
{
    uint32_t level = 0;
    err_t ret = ESP_OK;
    while (1)
    {
        xQueueReceive(power_vusb_isr_queue, &level, portMAX_DELAY);
        if (level == 1)
        {
            ESP_LOGI(TAG, "USB connected");
            esp_pm_lock_acquire(gpio_vusb_lock);
        }
        else
        {
            ESP_LOGI(TAG, "USB disconnected");
            esp_pm_lock_release(gpio_vusb_lock);
        }
    }
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
        .atten = ADC_ATTEN_DB_12,
        .bitwidth = ADC_BITWIDTH_DEFAULT,
    };

    ret = adc_oneshot_config_channel(adc_handle, adc_channel, &config);
    if (ret != ESP_OK)
    {
        ESP_LOGE(TAG, "Failed to init %s ADC: %s", name, esp_err_to_name(ret));
    }

    bool cal = gpio_adc_calibration_init(ADC_UNIT_1, adc_channel, ADC_ATTEN_DB_12, out_adc_cali_handle);
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
    return (float)vUSB / 1000;
}

void gpio_pairing_button_task(void *pvParameters)
{
    uint32_t startPushTime = 0;
    uint8_t lastState = 1;
    uint32_t pushTime = MILLIS - startPushTime;
    uint8_t push_from_boot = 0;
    connectivity_t current_mode_led = MODE_NONE;
    uint8_t pairingState = 0;
    uint32_t io_num = 0;
    while (1)
    {

        if (lastState == 1)
        {
            // wait for button push
            gpio_intr_enable(PAIRING_PIN);
            esp_pm_lock_release(gpio_pairing_lock);

            xQueueReceive(gpio_pairing_button_isr_queue, &io_num, portMAX_DELAY);
            esp_pm_lock_acquire(gpio_pairing_lock);
        }

        if (gpio_get_level(PAIRING_PIN) == 0) // if button is pushed
        {
            if (lastState == 1)
            {
                ESP_LOGI(TAG, "Start pushing %ld", MILLIS);
                suspendTask(tuyaTaskHandle);
                suspendTask(noConfigLedTaskHandle);
                lastState = 0;
                current_mode_led = MODE_NONE;
                startPushTime = MILLIS;
                if (MILLIS < 1000)
                {
                    push_from_boot = 1;
                }
            }
            pushTime = MILLIS - startPushTime;
            if (pushTime > 4000)
            {
                // Color Wheel
                ESP_LOGI(TAG, "pushTime: %lu, %%: %lu", pushTime, pushTime % 1000);
                if (pushTime % 1000 < 100)
                {
                    ESP_LOGI(TAG, "------------------------LED state before compute: %d", current_mode_led);
                    current_mode_led++;
                    if (current_mode_led == MODE_MQTT)
                    {
                        // skip MQTT_HA
                        current_mode_led++;
                    }
                    if (current_mode_led >= MODE_LAST)
                    {
                        current_mode_led = MODE_WEB;
                    }
                    ESP_LOGI(TAG, "------------------------LED state: %d", current_mode_led);
                    gpio_set_led_color(color_mode[current_mode_led]);
                }
            }
            else if (pushTime > 2000)
            {
                // Announce pairing mode
                if (push_from_boot)
                {
                    // factory reset
                    ESP_LOGI(TAG, "Factory reset");
                    gpio_start_led_pattern(PATTERN_FACTORY_RESET);
                    config_factory_reset();
                }
                if (current_mode_led == MODE_NONE)
                {
                    // flash pairing led
                    current_mode_led = config_values.mode;
                    suspendTask(noConfigLedTaskHandle);
                    // gpio_start_led_pattern(PATTERN_PAIRING);
                    gpio_boot_led_pattern();
                }
            }
            else
            {
                current_mode_led = MODE_NONE;
            }
        }
        else // if button is not pushed
        {
            if (lastState == 0) // button was released
            {
                push_from_boot = 0;
                ESP_LOGI(TAG, "Button pushed for %lu ms", pushTime);
                if (pushTime > 5000)
                {
                    if (noConfigLedTaskHandle != NULL)
                    {
                        vTaskDelete(noConfigLedTaskHandle);
                        noConfigLedTaskHandle = NULL;
                    }
                    ESP_LOGI(TAG, "Changing mode");
                    config_values.mode = current_mode_led;
                    config_write();
                    vTaskDelay(2000 / portTICK_PERIOD_MS);
                    esp_restart();
                }
                else if (pushTime > 2000 && pushTime <= 4000)
                {
                    ESP_LOGI(TAG, "Pairing mode");
                    xTaskCreate(gpio_led_task_pairing, "gpio_led_task_pairing", 2048, NULL, PRIORITY_LED_PAIRING, &gpio_led_pairing_task_handle);
                    if (pairingState)
                    {
                        // already in pairing mode
                        esp_restart();
                    }
                    pairingState = 1;
                    gpio_start_pariring();
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
                        xTaskCreate(ota_perform_task, "ota_perform_task", 16 * 1024, NULL, PRIORITY_OTA, NULL);
                    }
                    else
                    {
                        gpio_boot_led_pattern();
                        ESP_LOGI(TAG, "No action");
                        vTaskDelay(500 / portTICK_PERIOD_MS);
                        resumeTask(noConfigLedTaskHandle);
                        resumeTask(tuyaTaskHandle);
                    }
                }
                lastState = 1;
                current_mode_led = MODE_NONE;
            }
        }
        vTaskDelay(100 / portTICK_PERIOD_MS);
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
    xTaskCreate(gpio_led_pattern_task, "gpio_led_pattern_task", 4096, &lastPattern, PRIORITY_LED_PATTERN, &ledPatternTaskHandle);
}

void gpio_boot_led_pattern()
{

    gpio_set_led_color(color_mode[config_values.mode]);
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
    while (wifi_state == WIFI_CONNECTING && MILLIS < timout)
    {
        gpio_set_led_color(color_mode[MODE_WEB]);
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
    pattern_in_progress = 1;
    while (1)
    {
        gpio_set_led_color(color_mode[config_values.mode]);
        vTaskDelay(100 / portTICK_PERIOD_MS);
        gpio_set_led_color(0);
        vTaskDelay(900 / portTICK_PERIOD_MS);
        if (config_values.mode == MODE_ZIGBEE && config_values.zigbee.state == ZIGBEE_PAIRED)
        {
            break;
        }
    }
    pattern_in_progress = 0;
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

void gpio_start_pariring()
{
    ESP_LOGI(TAG, "Starting pairing");
    switch (config_values.mode)
    {
    case MODE_WEB:
    case MODE_MQTT:
    case MODE_MQTT_HA:
        ESP_LOGI(TAG, "Web pairing");
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
        ESP_LOGI(TAG, "Tuya pairing");
        xTaskCreate(tuya_pairing_task, "tuya_pairing_task", 8 * 1024, NULL, PRIORITY_TUYA, NULL);
        break;
    case MODE_ZIGBEE:
        ESP_LOGI(TAG, "Zigbee pairing");
        ESP_LOGI(TAG, "Already paired, resetting");
        config_values.zigbee.state = ZIGBEE_WANT_PAIRING;
        config_write();
        esp_zb_factory_reset();
        // if (config_values.zigbee.state == ZIGBEE_PAIRED)
        // {
        // }
        // else
        // {
        //     ESP_LOGI(TAG, "Not paired, starting pairing");
        //     config_values.zigbee.state = ZIGBEE_PAIRING;
        //     zigbee_start_pairing();
        // }
        // esp_zb_factory_reset();
        // start_zigbee_pairing();
        break;
    default:
        ESP_LOGI(TAG, "No pairing mode");
        break;
    }
}