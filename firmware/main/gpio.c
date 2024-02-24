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
#include "esp_sleep.h"

#include "gpio.h"
#include "config.h"
#include "wifi.h"
#include "main.h"
#include "zigbee.h"
#include "tuya.h"
#include "ota.h"
#include "power.h"
#include "led.h"
#include "shell.h"

/*==============================================================================
 Local Define
===============================================================================*/
#define TAG "GPIO"

/*==============================================================================
 Local Macro
===============================================================================*/

/*==============================================================================
 Local Type
===============================================================================*/

/*==============================================================================
 Local Function Declaration
===============================================================================*/
static bool gpio_adc_calibration_init(adc_unit_t unit, adc_channel_t channel, adc_atten_t atten, adc_cali_handle_t *out_handle);
static void gpio_init_adc(adc_unit_t adc_unit, adc_oneshot_unit_handle_t *out_handle);
static void gpio_init_adc_cali(adc_oneshot_unit_handle_t adc_handle, adc_channel_t adc_channel, adc_cali_handle_t *out_adc_cali_handle, char *name);
static void gpio_pairing_isr_cb(void *arg);
static void gpio_vusb_isr_cb(void *arg);
static void gpio_vusb_task(void *pvParameter);
static void gpio_init_vusb();

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

static QueueHandle_t gpio_pairing_button_isr_queue = NULL;
static QueueHandle_t power_vusb_isr_queue = NULL;
static volatile uint8_t vusb_level = 0;

static esp_pm_lock_handle_t gpio_pairing_lock = NULL;
static esp_pm_lock_handle_t gpio_vusb_lock = NULL;

/*==============================================================================
Function Implementation
===============================================================================*/

void gpio_init_pins()
{

    led_init();

    gpio_init_adc(ADC_UNIT_1, &adc1_handle);
    gpio_init_adc_cali(adc1_handle, V_USB_PIN, &adc_usb_cali_handle, "VUSB");
    gpio_init_adc_cali(adc1_handle, V_CONDO_PIN, &adc_capa_cali_handle, "VCondo");

    esp_pm_lock_create(ESP_PM_NO_LIGHT_SLEEP, 0, "gpio_pairing_lock", &gpio_pairing_lock);
    esp_pm_lock_create(ESP_PM_CPU_FREQ_MAX, 0, "gpio_vusb_lock", &gpio_vusb_lock);
    gpio_pairing_button_isr_queue = xQueueCreate(10, sizeof(uint32_t));
    power_vusb_isr_queue = xQueueCreate(1, sizeof(uint32_t));

    gpio_config_t pairing_conf = {
        .pin_bit_mask = (1ULL << PAIRING_PIN),
        .mode = GPIO_MODE_INPUT,
        .pull_up_en = GPIO_PULLUP_DISABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_LOW_LEVEL, // light sleep dont support edges
    };
    gpio_config(&pairing_conf);
    if (gpio_get_level(PAIRING_PIN) == 0)
    {
        ESP_LOGI(TAG, "Pairing button pushed at boot");
        gpio_intr_disable(PAIRING_PIN);
        uint32_t pin = PAIRING_PIN;
        xQueueSend(gpio_pairing_button_isr_queue, &pin, 0);
    }

    gpio_init_vusb();

    gpio_install_isr_service(0);
    gpio_isr_handler_add(PAIRING_PIN, gpio_pairing_isr_cb, (void *)PAIRING_PIN);
    gpio_isr_handler_add(V_USB_PIN, gpio_vusb_isr_cb, (void *)V_USB_PIN);

    // gpio_set_direction(LED_DATA, GPIO_MODE_INPUT); // HIGH-Z
    ESP_LOGI(TAG, "VCondo: %fV", gpio_get_vcondo());

    gpio_wakeup_enable(PAIRING_PIN, GPIO_INTR_LOW_LEVEL);
    gpio_wakeup_enable(V_USB_PIN, vusb_level ? GPIO_INTR_LOW_LEVEL : GPIO_INTR_HIGH_LEVEL);
    esp_sleep_enable_gpio_wakeup();

    xTaskCreate(gpio_pairing_button_task, "gpio_pairing_button_task", 8192, NULL, PRIORITY_PAIRING, NULL); // start push button task
    xTaskCreate(gpio_vusb_task, "gpio_vusb_task", 4 * 1024, NULL, PRIORITY_PAIRING, NULL);                 // start push button task
}

static void gpio_init_vusb()
{
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
        esp_pm_lock_release(gpio_vusb_lock);
        esp_pm_lock_acquire(gpio_vusb_lock);
    }
    ESP_LOGI(TAG, "VUSB level: %d", vusb_level);
    vusb_conf.intr_type = vusb_level ? GPIO_INTR_LOW_LEVEL : GPIO_INTR_HIGH_LEVEL;
    gpio_config(&vusb_conf);
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
    esp_rom_printf("IT USB %ld lvl: %ld\n", gpio_num, level);
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
    vusb_level = level;
    gpio_intr_enable(gpio_num);
    xQueueOverwriteFromISR(power_vusb_isr_queue, &level, NULL);
}

static void gpio_vusb_task(void *pvParameter)
{
    uint32_t level = 0;
    uint32_t last_level = vusb_level;
    while (1)
    {
        xQueueReceive(power_vusb_isr_queue, &level, portMAX_DELAY);
        level = gpio_get_level(V_USB_PIN);
        if (level != last_level)
        {
            last_level = level;

            if (level == 1)
            {
                ESP_LOGI(TAG, "USB connected");
                esp_pm_lock_acquire(gpio_vusb_lock);
                if (config_values.mode == MODE_ZIGBEE)
                {
                    esp_zb_sleep_enable(false);
                }
            }
            else
            {
                ESP_LOGI(TAG, "USB disconnected");
                esp_pm_lock_release(gpio_vusb_lock);
                if (config_values.mode == MODE_ZIGBEE)
                {
                    esp_zb_sleep_enable(true);
                    if (shell_repl != NULL)
                    {
                        ESP_LOGI(TAG, "USB unplugged, removing Shell...");
                        esp_restart();
                    }
                }
            }
            led_usb_event(level);
        }
        vTaskDelay(1000 / portTICK_PERIOD_MS);
    }
}

static void gpio_init_adc(adc_unit_t adc_unit, adc_oneshot_unit_handle_t *out_handle)
{
    esp_err_t ret = ESP_OK;

    if (out_handle != NULL && *out_handle != NULL)
    {
        adc_oneshot_del_unit(*out_handle);
    }

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

static bool gpio_adc_calibration_init(adc_unit_t unit, adc_channel_t channel, adc_atten_t atten, adc_cali_handle_t *out_handle)
{
    adc_cali_handle_t handle = NULL;
    esp_err_t ret = ESP_FAIL;
    bool calibrated = false;

#if ADC_CALI_SCHEME_CURVE_FITTING_SUPPORTED
    if (!calibrated)
    {
        if (out_handle != NULL && *out_handle != NULL)
        {
            adc_cali_delete_scheme_curve_fitting(*out_handle);
        }

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
        if (out_handle != NULL)
        {
            adc_cali_delete_scheme_line_fitting(*out_handle);
        }

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

uint8_t gpio_vusb_connected()
{
    // gpio_init_vusb();
    return gpio_get_level(V_USB_PIN);
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
                if (push_from_boot)
                {
                    // factory reset
                    ESP_LOGI(TAG, "Factory reset");
                    led_start_pattern(LED_FACTORY_RESET);
                    config_factory_reset();
                }
                else if (pushTime % 1000 < 100)
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
                    led_start_pattern(LED_COLOR_WHEEL);
                    led_set_color(led_color_mode[current_mode_led]);
                }
            }
            else if (pushTime > 2000)
            {
                // Announce pairing mode
                if (push_from_boot)
                {
                    // factory reset advert
                    ESP_LOGI(TAG, "Factory reset advert");
                    led_start_pattern(LED_FACTORY_RESET_ADVERT);
                }
                if (current_mode_led == MODE_NONE)
                {
                    // flash pairing led
                    current_mode_led = config_values.mode;
                    led_start_pattern(LED_BOOT);
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
                if (push_from_boot)
                {
                    led_stop_pattern(LED_FACTORY_RESET_ADVERT);
                    led_stop_pattern(LED_FACTORY_RESET);
                    push_from_boot = 0;
                    lastState = 1;
                    current_mode_led = MODE_NONE;
                    continue;
                }

                ESP_LOGI(TAG, "Button pushed for %lu ms", pushTime);
                if (pushTime > 5000)
                {
                    led_stop_pattern(LED_COLOR_WHEEL);
                    ESP_LOGI(TAG, "Changing mode");
                    config_values.mode = current_mode_led;
                    config_write();
                    vTaskDelay(2000 / portTICK_PERIOD_MS);
                    esp_restart();
                }
                else if (pushTime > 2000 && pushTime <= 4000)
                {
                    ESP_LOGI(TAG, "Pairing mode");
                    led_start_pattern(LED_PAIRING);
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
                    if (ota_state == OTA_AVAILABLE && gpio_vusb_connected())
                    {
                        ESP_LOGI(TAG, "OTA available, starting update");
                        suspendTask(main_task_handle);
                        resumeTask(tuyaTaskHandle);
                        ota_state = OTA_INSTALLING;
                        vTaskDelay(500 / portTICK_PERIOD_MS); // wait for led task to update
                        xTaskCreate(ota_perform_task, "ota_perform_task", 16 * 1024, NULL, PRIORITY_OTA, NULL);
                    }
                    else
                    {
                        led_start_pattern(LED_BOOT);
                        if (config_values.mode == MODE_ZIGBEE)
                        {
                            ESP_LOGI(TAG, "Zigbee send value");
                            main_sleep_time = 1;
                        }
                        else
                        {
                            ESP_LOGI(TAG, "No action");
                        }
                        vTaskDelay(500 / portTICK_PERIOD_MS);
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

void gpio_start_pariring()
{
    ESP_LOGI(TAG, "Starting pairing");
    switch (config_values.mode)
    {
    case MODE_WEB:
    case MODE_MQTT:
    case MODE_MQTT_HA:
        ESP_LOGI(TAG, "Web pairing");
        suspendTask(main_task_handle);
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
        zigbee_factory_reset();
        ESP_LOGI(TAG, "Already paired, resetting");
        config_values.zigbee.state = ZIGBEE_WANT_PAIRING;
        config_write();
        hard_restart();
        // esp_zb_factory_reset();
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

void gpio_peripheral_reinit()
{
    gpio_init_adc(ADC_UNIT_1, &adc1_handle);
    // gpio_init_adc_cali(adc1_handle, V_USB_PIN, &adc_usb_cali_handle, "VUSB");
    gpio_init_adc_cali(adc1_handle, V_CONDO_PIN, &adc_capa_cali_handle, "VCondo");
    led_init();
    linky_init(MODE_HIST, RX_LINKY);
}