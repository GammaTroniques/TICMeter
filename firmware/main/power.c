/**
 * @file power.c
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

#include "esp_pm.h"
#include "esp_private/esp_clk.h"
#include "esp_log.h"
#include "time.h"
#include <unistd.h>
#include "esp_timer.h"

#include "power.h"
#include "common.h"
#include "gpio.h"
/*==============================================================================
 Local Define
===============================================================================*/

/*==============================================================================
 Local Macro
===============================================================================*/
#define TAG "POWER"
/*==============================================================================
 Local Type
===============================================================================*/

/*==============================================================================
 Local Function Declaration
===============================================================================*/
static void power_vusb_task(void *pvParameter);

/*==============================================================================
Public Variable
===============================================================================*/
QueueHandle_t power_vusb_isr_queue = NULL;

/*==============================================================================
 Local Variable
===============================================================================*/

/*==============================================================================
Function Implementation
===============================================================================*/

int enter_time = 0;
esp_err_t power_sleep_enter()
{
    esp_err_t ret = ESP_OK;
    enter_time = esp_timer_get_time();
    return ret;
}

esp_err_t power_sleep_leave()
{
    esp_err_t ret = ESP_OK;
    int delta = esp_timer_get_time() - enter_time;
    esp_rom_printf("Leaving %ld\n", delta);
    return ret;
}

esp_err_t power_init()
{
    esp_err_t ret = ESP_OK;
    int default_cpu_freq_mhz = CONFIG_ESP_DEFAULT_CPU_FREQ_MHZ;

    esp_pm_config_t pm_config = {
        .max_freq_mhz = default_cpu_freq_mhz,
        .min_freq_mhz = 10,
#if CONFIG_FREERTOS_USE_TICKLESS_IDLE
        .light_sleep_enable = true
#endif
    };
    ret = esp_pm_configure(&pm_config);
    if (ret != ESP_OK)
    {
        ESP_LOGE(TAG, "PM init failed: 0x%x", ret);
    }
    else
    {
        ESP_LOGI(TAG, "PM initialized");
    }

    esp_pm_sleep_cbs_register_config_t cbs = {
        .enter_cb = power_sleep_enter,
        .exit_cb = power_sleep_leave,
    };
    ret = esp_pm_light_sleep_register_cbs(&cbs);

    power_vusb_isr_queue = xQueueCreate(10, sizeof(uint32_t));

    xTaskCreate(power_vusb_task, "power_vusb_task", 2048, NULL, 10, NULL);

    return ret;
}

esp_err_t power_set_frequency(uint32_t freq_Mhz)
{
    esp_err_t ret = ESP_OK;
    esp_pm_config_t pm_config = {
        .max_freq_mhz = freq_Mhz,
        .min_freq_mhz = freq_Mhz,
    };
    ret = esp_pm_configure(&pm_config);
    if (ret != ESP_OK)
    {
        ESP_LOGE(TAG, "esp_pm_configure failed");
    }
    else
    {
        ESP_LOGI(TAG, "Frequency set to %ld MHz", freq_Mhz);
    }
    return ret;
}

static void power_vusb_task(void *pvParameter)
{
    uint32_t io_num = 0;
    while (1)
    {
        xQueueReceive(power_vusb_isr_queue, &io_num, portMAX_DELAY);
        if (gpio_get_level(V_USB_PIN) == 1)
        {
            ESP_LOGI(TAG, "USB connected");
        }
        else
        {
            ESP_LOGI(TAG, "USB disconnected");
        }
        vTaskDelay(1000 / portTICK_PERIOD_MS);
    }
}