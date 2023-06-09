#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/event_groups.h"
#include "driver/gpio.h"
#include "esp_adc/adc_oneshot.h"
#include "esp_adc/adc_cali.h"
#include "esp_adc/adc_cali_scheme.h"
#include "esp_log.h"

#include "gpio.h"
#include "config.h"
#include "wifi.h"
#include "main.h"

#define TAG "GPIO"

adc_oneshot_unit_handle_t adc1_handle;
void initPins()
{
    gpio_reset_pin(LED_RED);
    gpio_reset_pin(LED_GREEN);
    gpio_set_direction(LED_RED, GPIO_MODE_OUTPUT);
    gpio_set_direction(LED_GREEN, GPIO_MODE_OUTPUT);

    gpio_set_level(LED_RED, 0);
    gpio_set_level(LED_GREEN, 0);

    // adc_oneshot_unit_init_cfg_t init_config1 = {};
    // init_config1.unit_id = ADC_UNIT_1;

    // adc_oneshot_chan_cfg_t config = {
    //     .atten = ADC_ATTEN_DB_0,
    //     .bitwidth = ADC_BITWIDTH_DEFAULT,
    // };
    // ESP_ERROR_CHECK(adc_oneshot_new_unit(&init_config1, &adc1_handle));
    // ESP_ERROR_CHECK(adc_oneshot_config_channel(adc1_handle, V_CONDO_PIN, &config));
}

uint8_t getVUSB()
{
    // return adc1_get_raw(ADC1_CHANNEL_3) > 3700 ? 1 : 0;
    return gpio_get_level(V_USB_PIN);
}

float getVCondo()
{
    adc_oneshot_unit_init_cfg_t init_config1 = {};
    init_config1.unit_id = ADC_UNIT_1;

    adc_oneshot_chan_cfg_t config = {
        .atten = ADC_ATTEN_DB_0,
        .bitwidth = ADC_BITWIDTH_DEFAULT,
    };
    ESP_ERROR_CHECK(adc_oneshot_new_unit(&init_config1, &adc1_handle));
    ESP_ERROR_CHECK(adc_oneshot_config_channel(adc1_handle, V_CONDO_PIN, &config));

    int raw = 0;
    // adc_oneshot_io_to_channel
    ESP_ERROR_CHECK(adc_oneshot_read(adc1_handle, V_CONDO_PIN, &raw));
    ESP_LOGI(TAG, "VCondo raw: %d", raw);
    float vCondo = (float)(raw * 5) / 3988; // get VCondo from ADC after voltage divider
    adc_oneshot_del_unit(adc1_handle);
    return vCondo;
}

void led_blink_task(void *pvParameter)
{
    gpio_reset_pin(LED_RED);
    gpio_reset_pin(LED_GREEN);
    gpio_set_direction(LED_RED, GPIO_MODE_OUTPUT);
    gpio_set_direction(LED_GREEN, GPIO_MODE_OUTPUT);
    uint8_t led_red_state = 0;
    while (1)
    {
        // ESP_LOGI(TAG, "Turning the LED %s!", led_red_state == true ? "ON" : "OFF");
        // ESP_LOGI(TAG, "%lld", esp_log_timestamp());
        // ESP_LOGI(TAG, "%lu", xTaskGetTickCount());
        // ESP_LOGI(TAG, "%lu", );

        gpio_set_level(LED_RED, led_red_state);
        led_red_state = !led_red_state;
        vTaskDelay(950 / portTICK_PERIOD_MS);

        gpio_set_level(LED_RED, led_red_state);
        led_red_state = !led_red_state;
        vTaskDelay(50 / portTICK_PERIOD_MS);

        // gpio_set_level(LED_GREEN, getVUSB());
    }
}

void loop(void *arg)
{
    static int adc_raw[2][10];
    //-------------ADC1 Init---------------//
    adc_oneshot_unit_handle_t adc1_handle = NULL;
    adc_oneshot_unit_init_cfg_t init_config1 = {};
    init_config1.unit_id = ADC_UNIT_1;

    ESP_ERROR_CHECK(adc_oneshot_new_unit(&init_config1, &adc1_handle));

    //-------------ADC1 Config---------------//
    adc_oneshot_chan_cfg_t config = {};
    config.bitwidth = ADC_BITWIDTH_DEFAULT;
    config.atten = ADC_ATTEN_DB_11;

    ESP_ERROR_CHECK(adc_oneshot_config_channel(adc1_handle, ADC_CHANNEL_5, &config));
    while (1)
    {
        ESP_ERROR_CHECK(adc_oneshot_read(adc1_handle, ADC_CHANNEL_5, &adc_raw[0][0]));
        ESP_LOGI(TAG, "ADC%d Channel[%d] Raw Data: %d", ADC_UNIT_1 + 1, ADC_CHANNEL_5, adc_raw[0][0]);
        vTaskDelay(pdMS_TO_TICKS(1000));
    }
}

void pairingButtonTask(void *pvParameters)
{
    uint32_t startPushTime = 0;
    uint8_t lastState = 1;
    uint32_t pushTime = MILLIS - startPushTime;
    uint8_t ledState = 0;
    while (1)
    {
        if (gpio_get_level(PAIRING_PIN) == 0) // if button is pushed
        {
            if (lastState == 1)
            {
                gpio_set_level(LED_GREEN, 1);
                vTaskDelay(10 / portTICK_PERIOD_MS);
                gpio_set_level(LED_GREEN, 0);
            }
            lastState = 0;
            pushTime = MILLIS - startPushTime;
            if (pushTime > 5000)
            {
                ledState = !ledState;
                ESP_LOGI(TAG, "%d", ledState);
                gpio_set_level(LED_RED, ledState);
                gpio_set_level(LED_GREEN, 0);
            }
            else if (pushTime > 2000)
            {
                ledState = !ledState;
                gpio_set_level(LED_RED, 0);
                gpio_set_level(LED_GREEN, ledState);
            }
            else
            {
                gpio_set_level(LED_RED, 0);
                gpio_set_level(LED_GREEN, 0);
            }
        }
        else
        {
            if (lastState == 0)
            {
                // ESP_LOGI(MAIN_TAG, "Button pushed for %lu ms", pushTime);
                if (pushTime > 5000)
                {
                    ESP_LOGI(TAG, "Changing mode");
                    switch (config.values.mode)
                    {
                    case MODE_WEB:
                    case MODE_MQTT:
                    case MODE_MQTT_HA:
                        ESP_LOGI(TAG, "Changing to zigbee");
                        config.values.mode = MODE_ZIGBEE;
                        gpio_set_level(LED_RED, 1);
                        gpio_set_level(LED_GREEN, 0);
                        break;
                    case MODE_ZIGBEE:
                        ESP_LOGI(TAG, "Changing to web");
                        config.values.mode = MODE_WEB;
                        gpio_set_level(LED_RED, 0);
                        gpio_set_level(LED_GREEN, 1);
                        break;
                    default:
                        ESP_LOGI(TAG, "Changing to web");
                        config.values.mode = MODE_WEB;
                        gpio_set_level(LED_RED, 0);
                        gpio_set_level(LED_GREEN, 1);
                        break;
                    }
                    config.write();
                    vTaskDelay(5000 / portTICK_PERIOD_MS);
                    esp_restart();
                }
                else if (pushTime > 2000 && pushTime <= 5000)
                {
                    ESP_LOGI(TAG, "Pairing mode");
                    switch (config.values.mode)
                    {
                    case MODE_WEB:
                    case MODE_MQTT:
                    case MODE_MQTT_HA:
                        ESP_LOGI(TAG, "Web pairing");
                        vTaskSuspend(fetchLinkyDataTaskHandle);
                        vTaskSuspend(sendDataTaskHandle);
                        if (wifiConnected)
                        {
                            disconectFromWifi();
                            vTaskDelay(1000 / portTICK_PERIOD_MS);
                        }
                        start_captive_portal();

                        break;
                    case MODE_ZIGBEE:
                        ESP_LOGI(TAG, "Zigbee pairing TODO");
                        // start_zigbee_pairing();
                        break;
                    default:
                        ESP_LOGI(TAG, "No pairing mode");
                        break;
                    }
                }
                else
                {
                    ESP_LOGI(TAG, "No action");
                }
                gpio_set_level(LED_RED, 0);
                gpio_set_level(LED_GREEN, 0);
            }
            lastState = 1;
            startPushTime = MILLIS;
        }
        vTaskDelay(50 / portTICK_PERIOD_MS);
    }
}

struct ledBlinkParams_t
{
    gpio_num_t led;
    uint32_t tOn;
    uint32_t tOff;
    uint32_t count;
};

void ledPatternTask(void *pvParameters)
{
    ledBlinkParams_t *params = (ledBlinkParams_t *)pvParameters;
    for (int i = 0; i < params->count; i++)
    {
        gpio_set_level(params->led, 1);
        vTaskDelay(params->tOn / portTICK_PERIOD_MS);
        gpio_set_level(params->led, 0);
        vTaskDelay(params->tOff / portTICK_PERIOD_MS);
    }
    gpio_set_level(params->led, 0);
    vTaskDelete(NULL);
    free(params);
}

void startLedPattern(gpio_num_t led, uint8_t count, uint16_t tOn, uint16_t tOff)
{
    ledBlinkParams_t *params = (ledBlinkParams_t *)malloc(sizeof(ledBlinkParams_t));
    params->led = led;
    params->tOn = tOn;
    params->tOff = tOff;
    params->count = count;
    xTaskCreate(ledPatternTask, "ledPatternTask", 2048, (void *)params, 5, NULL);
}