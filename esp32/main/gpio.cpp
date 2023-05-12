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

    adc_oneshot_unit_init_cfg_t init_config1 = {
        .unit_id = ADC_UNIT_1,
    };
    adc_oneshot_chan_cfg_t config = {
        .atten = ADC_ATTEN_DB_0,
        .bitwidth = ADC_BITWIDTH_DEFAULT,
    };
    ESP_ERROR_CHECK(adc_oneshot_new_unit(&init_config1, &adc1_handle));
    ESP_ERROR_CHECK(adc_oneshot_config_channel(adc1_handle, V_CONDO_PIN, &config));
}

uint8_t getVUSB()
{
    // return adc1_get_raw(ADC1_CHANNEL_3) > 3700 ? 1 : 0;
    return gpio_get_level(V_USB_PIN);
}

float getVCondo()
{
    int raw = 0;
    // adc_oneshot_io_to_channel
    ESP_ERROR_CHECK(adc_oneshot_read(adc1_handle, V_CONDO_PIN, &raw));
    ESP_LOGI(TAG, "VCondo raw: %d", raw);
    float vCondo = (float)(raw * 5) / 3988; // get VCondo from ADC after voltage divider
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
    while (1)
    {
        int raw = 0;
        adc_oneshot_read(adc1_handle, V_CONDO_PIN, &raw);
        ESP_LOGI("LOOP", "VCondo: %d", raw);
        vTaskDelay(1000 / portTICK_PERIOD_MS);
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