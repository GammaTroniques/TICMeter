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
#include "esp_sleep.h"

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

/*==============================================================================
Public Variable
===============================================================================*/

/*==============================================================================
 Local Variable
===============================================================================*/

/*==============================================================================
Function Implementation
===============================================================================*/

int enter_time = 0;
int exit_time = 0;
esp_err_t power_sleep_enter()
{
    esp_err_t ret = ESP_OK;
    enter_time = esp_timer_get_time();
    esp_rom_printf("entering sleep after %d ms\n", abs(enter_time - exit_time) / 1000);
    return ret;
}

esp_err_t power_sleep_leave()
{
    esp_err_t ret = ESP_OK;
    int delta = esp_timer_get_time() - enter_time;
    exit_time = esp_timer_get_time();
    const char *wakeup_reason;
    switch (esp_sleep_get_wakeup_cause())
    {
    case ESP_SLEEP_WAKEUP_TIMER:
        wakeup_reason = "timer";
        break;
    case ESP_SLEEP_WAKEUP_GPIO:
        wakeup_reason = "pin";
        break;
    case ESP_SLEEP_WAKEUP_UART:
        wakeup_reason = "uart";
        /* Hang-up for a while to switch and execuse the uart task
         * Otherwise the chip may fall sleep again before running uart task */
        vTaskDelay(1);
        break;
#if CONFIG_IDF_TARGET_ESP32S2 || CONFIG_IDF_TARGET_ESP32S3
    case ESP_SLEEP_WAKEUP_TOUCHPAD:
        wakeup_reason = "touch";
        break;
#endif
    default:
        wakeup_reason = "other";
        break;
    }
    esp_rom_printf("Wake %s after %d ms\n", wakeup_reason, delta / 1000);
    return ret;
}

esp_err_t power_init()
{
    esp_err_t ret = ESP_OK;
    esp_pm_config_t pm_config = {
        .max_freq_mhz = CONFIG_ESP_DEFAULT_CPU_FREQ_MHZ,
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

    // esp_pm_sleep_cbs_register_config_t cbs = {
    //     .enter_cb = power_sleep_enter,
    //     .exit_cb = power_sleep_leave,
    // };
    // ret = esp_pm_light_sleep_register_cbs(&cbs);

    return ret;
}

esp_err_t power_set_zigbee()
{
    esp_err_t ret = ESP_OK;
    esp_pm_config_t pm_config = {
        .max_freq_mhz = CONFIG_ESP_DEFAULT_CPU_FREQ_MHZ,
        .min_freq_mhz = CONFIG_ESP_DEFAULT_CPU_FREQ_MHZ,
#if CONFIG_FREERTOS_USE_TICKLESS_IDLE
        .light_sleep_enable = true
#endif
    };
    ret = esp_pm_configure(&pm_config);
    if (ret != ESP_OK)
    {
        ESP_LOGE(TAG, "PM Zigbee init failed: 0x%x", ret);
    }
    else
    {
        ESP_LOGI(TAG, "PM Zigbee initialized");
    }
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

uint32_t power_get_frequency()
{

    return esp_clk_cpu_freq();
}