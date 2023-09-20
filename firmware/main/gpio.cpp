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

#define TAG "GPIO"

adc_oneshot_unit_handle_t adc1_handle;

led_strip_handle_t led;

enum led_state_t
{
    NO_FLASH,
    PAIRING_FLASH,
    WEB_FLASH = 0x0008FF,
    ZIGBEE_FLASH = 0xFF0000,
    TUYA_FLASH = 0xFA650F,
};

void initPins()
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
    // gpio_set_direction(LED_DATA, GPIO_MODE_INPUT); // HIGH-Z
}

void setLedColor(uint32_t color)
{
    if (color == 0)
    {
        led_strip_clear(led);
        led_strip_refresh(led);
        gpio_set_level(LED_EN, 0);
        // gpio_set_direction(LED_DATA, GPIO_MODE_INPUT); // HIGH-Z
        return;
    }
    gpio_set_level(LED_EN, 1);
    // gpio_set_direction(LED_DATA, GPIO_MODE_OUTPUT);
    uint8_t r = (color >> 16) & 0xFF;
    uint8_t g = (color >> 8) & 0xFF;
    uint8_t b = (color >> 0) & 0xFF;
    // set brightness
    uint8_t brightness = 5; // %
    r = (r * brightness) / 100;
    g = (g * brightness) / 100;
    b = (b * brightness) / 100;

    led_strip_set_pixel(led, 0, r, g, b);
    led_strip_refresh(led);
}

static bool example_adc_calibration_init(adc_unit_t unit, adc_channel_t channel, adc_atten_t atten, adc_cali_handle_t *out_handle)
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
float getVCondo()
{
    esp_log_level_set("ADC", ESP_LOG_DEBUG);
    adc_oneshot_unit_init_cfg_t init_config1 = {};
    init_config1.unit_id = ADC_UNIT_1;

    adc_oneshot_chan_cfg_t config = {
        .atten = ADC_ATTEN_DB_11,
        .bitwidth = ADC_BITWIDTH_DEFAULT,
    };
    ESP_ERROR_CHECK(adc_oneshot_new_unit(&init_config1, &adc1_handle));
    ESP_ERROR_CHECK(adc_oneshot_config_channel(adc1_handle, V_CONDO_PIN, &config));

    adc_cali_handle_t adc_cali_handle = NULL;
    bool do_calibration = example_adc_calibration_init(ADC_UNIT_1, V_CONDO_PIN, ADC_ATTEN_DB_11, &adc_cali_handle);

    int raw = 0;
    ESP_ERROR_CHECK(adc_oneshot_read(adc1_handle, V_CONDO_PIN, &raw));
    adc_oneshot_del_unit(adc1_handle);
    int vADC = 0;
    if (do_calibration)
    {
        ESP_ERROR_CHECK(adc_cali_raw_to_voltage(adc_cali_handle, raw, &vADC));
        uint32_t vCondo = (vADC * 280) / 180;
        ESP_LOGD("ADC", "VUSB: %ld", vCondo);
        return (float)vCondo / 1000;
    }
    // float vCondo = (float)(raw * 5) / 3988; // get VCondo from ADC after voltage divider
    return 0.0;
}

float getVUSB()
{
    adc_oneshot_unit_init_cfg_t init_config1 = {};
    init_config1.unit_id = ADC_UNIT_1;

    adc_oneshot_chan_cfg_t config = {
        .atten = ADC_ATTEN_DB_11,
        .bitwidth = ADC_BITWIDTH_DEFAULT,
    };
    ESP_ERROR_CHECK(adc_oneshot_new_unit(&init_config1, &adc1_handle));
    ESP_ERROR_CHECK(adc_oneshot_config_channel(adc1_handle, V_USB_PIN, &config));

    adc_cali_handle_t adc_cali_handle = NULL;
    bool do_calibration = example_adc_calibration_init(ADC_UNIT_1, V_USB_PIN, ADC_ATTEN_DB_11, &adc_cali_handle);

    int raw = 0;
    ESP_ERROR_CHECK(adc_oneshot_read(adc1_handle, V_USB_PIN, &raw));
    adc_oneshot_del_unit(adc1_handle);
    int vADC = 0;
    if (do_calibration)
    {
        ESP_ERROR_CHECK(adc_cali_raw_to_voltage(adc_cali_handle, raw, &vADC));
        uint32_t vUSB = (vADC * 280) / 180;
        ESP_LOGD("ADC", "VUSB: %ld", vUSB);
        return (float)vUSB / 1000;
    }
    // float vUSB = (float)(raw * 5) / 3988; // get VUSB from ADC after voltage divider
    return 0.0;
}

void pairingButtonTask(void *pvParameters)
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
                    setLedColor(ledState);
                }
            }
            else if (pushTime > 2000)
            {
                // Announce pairing mode
                if (ledState == NO_FLASH)
                {
                    ledState = PAIRING_FLASH; // flash pairing led
                    suspendTask(noConfigLedTaskHandle);
                    startLedPattern(PATTERN_PAIRING);
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
                        config.values.mode = MODE_WEB;
                        break;
                    case ZIGBEE_FLASH:
                        config.values.mode = MODE_ZIGBEE;
                        break;
                    case TUYA_FLASH:
                        config.values.mode = MODE_TUYA;
                        break;
                    default:
                        break;
                    }
                    config.write();
                    vTaskDelay(2000 / portTICK_PERIOD_MS);
                    esp_restart();
                }
                else if (pushTime > 2000 && pushTime <= 4000)
                {
                    ESP_LOGI(TAG, "Pairing mode");
                    xTaskCreate(pairingLedTask, "pairingLedTask", 2048, NULL, 5, NULL);
                    if (pairingState)
                    {
                        // already in pairing mode
                        esp_restart();
                    }
                    switch (config.values.mode)
                    {
                    case MODE_WEB:
                    case MODE_MQTT:
                    case MODE_MQTT_HA:
                    case MODE_TUYA:
                        ESP_LOGI(TAG, "Web pairing");
                        pairingState = 1;
                        suspendTask(fetchLinkyDataTaskHandle);
                        if (wifiConnected)
                        {
                            disconectFromWifi();
                            vTaskDelay(1000 / portTICK_PERIOD_MS);
                        }
                        ESP_LOGI(TAG, "Starting captive portal");
                        start_captive_portal();
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
                    resumeTask(noConfigLedTaskHandle);
                    resumeTask(tuyaTaskHandle);
                    ESP_LOGI(TAG, "No action");
                }
                lastState = 1;
                ledState = NO_FLASH;
            }
        }
        vTaskDelay(50 / portTICK_PERIOD_MS);
    }
}

typedef struct ledPattern_t
{
    uint32_t color;
    uint32_t tOn;
    uint32_t tOff;
} ledPattern_t;

#define PATTERN_SIZE 3

// clang-format off
const ledPattern_t ledPattern[][PATTERN_SIZE] = {
    {{0x0008FF,      100, 400}                                                          }, // WIFI_CONNECTING // TODO: remove 
    {{0xFF8000,      200, 100}                                                          }, // WIFI_RETRY, new try // TODO: remove 
    {{0xFF0000,      200, 100},                                                         }, // WIFI_FAILED
    {{0x5EFF00,      500, 100}                                                          }, // LINKY_OK // TODO: remove 
    {{0xFF00F2,     1000, 100}                                                          }, // LINKY_ERR
    {{0x00FF00,      200, 500},  {0x00FF00,      200, 500}                              }, // SEND_OK
    {{0xFF0000,      200, 1000}, {0xFF0000,      200, 1000}                             }, // SEND_ERR
    {{0xFF0000,       50, 100},  {0xFF0000,       50, 100}, {0xFF0000,       50, 100   }}, // NO_CONFIG // TODO: remove 
    {{0xE5FF00,       50,   0}                                                          }, // START
    {{0x8803FC,       100,  400}                                                        }, // PAIRING
};
// clang-format on

void ledPatternTask(void *pvParameters)
{
    uint8_t id = *(uint8_t *)pvParameters;
    for (int i = 0; i < PATTERN_SIZE; i++)
    {
        setLedColor(ledPattern[id][i].color);
        vTaskDelay(ledPattern[id][i].tOn / portTICK_PERIOD_MS);
        setLedColor(0);
        vTaskDelay(ledPattern[id][i].tOff / portTICK_PERIOD_MS);
    }
    vTaskDelete(NULL); // Delete this task
}

void startLedPattern(uint8_t pattern)
{
    static TaskHandle_t ledPatternTaskHandle = NULL;
    static uint8_t lastPattern;
    lastPattern = pattern;
    xTaskCreate(ledPatternTask, "ledPatternTask", 2048, &lastPattern, 5, &ledPatternTaskHandle);
}

void startLedPattern()
{
    switch (config.values.mode)
    {
    case MODE_WEB:
    case MODE_MQTT:
    case MODE_MQTT_HA:
        setLedColor(WEB_FLASH);
        break;

    case MODE_TUYA:
        setLedColor(TUYA_FLASH);
        break;
    case MODE_ZIGBEE:
        setLedColor(ZIGBEE_FLASH);
        break;
    default:
        break;
    }
    vTaskDelay(100 / portTICK_PERIOD_MS);
    setLedColor(0);
}

void noConfigLedTask(void *pvParameters)
{
    while (config.verify())
    {
        vTaskDelay(5000 / portTICK_PERIOD_MS);
        for (int i = 0; i < 3; i++)
        {
            setLedColor(0xFF0000);
            vTaskDelay(50 / portTICK_PERIOD_MS);
            setLedColor(0);
            vTaskDelay(100 / portTICK_PERIOD_MS);
        }
    }
    vTaskDelete(NULL); // Delete this task
}

void wifiConnectLedTask(void *pvParameters)
{
    uint32_t timout = MILLIS + WIFI_CONNECT_TIMEOUT;
    while (!wifiConnected && MILLIS < timout)
    {
        setLedColor(WEB_FLASH);
        vTaskDelay(100 / portTICK_PERIOD_MS);
        setLedColor(0);
        vTaskDelay(900 / portTICK_PERIOD_MS);
    }
    vTaskDelete(NULL); // Delete this task
}

void linkyReadingLedTask(void *pvParameters)
{
    while (linky.reading)
    {
        setLedColor(0xFF8000);
        vTaskDelay(100 / portTICK_PERIOD_MS);
        setLedColor(0);
        vTaskDelay(900 / portTICK_PERIOD_MS);
    }
    vTaskDelete(NULL); // Delete this task
}

void sendingLedTask(void *pvParameters)
{
    while (sendingValues)
    {
        setLedColor(0xc300ff);
        vTaskDelay(100 / portTICK_PERIOD_MS);
        setLedColor(0);
        vTaskDelay(900 / portTICK_PERIOD_MS);
    }
    vTaskDelete(NULL); // Delete this task
}

void pairingLedTask(void *pvParameters)
{
    while (1)
    {
        setLedColor(0x8803FC);
        vTaskDelay(100 / portTICK_PERIOD_MS);
        setLedColor(0);
        vTaskDelay(900 / portTICK_PERIOD_MS);
    }
    vTaskDelete(NULL); // Delete this task
}