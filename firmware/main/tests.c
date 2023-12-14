/**
 * @file tests.c
 * @author Dorian Benech
 * @brief
 * @version 1.0
 * @date 2023-12-09
 *
 * @copyright Copyright (c) 2023 GammaTroniques
 *
 */

/*==============================================================================
 Local Include
===============================================================================*/
#include "tests.h"
#include "esp_log.h"
#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/event_groups.h"
#include "driver/gpio.h"
#include "gpio.h"
#include "driver/uart.h"
/*==============================================================================
 Local Define
===============================================================================*/
#define TAG "TESTS"
/*==============================================================================
 Local Macro
===============================================================================*/

/*==============================================================================
 Local Type
===============================================================================*/

/*==============================================================================
 Local Function Declaration
===============================================================================*/
static esp_err_t adc_test();
static void tests_task(void *pvParameters);

/*==============================================================================
Public Variable
===============================================================================*/
const char *const tests_available_tests[] = {
    [TEST_ALL] = "all",
    [TEST_ADC] = "adc",
};

const uint32_t tests_count = sizeof(tests_available_tests) / sizeof(char *);
/*==============================================================================
 Local Variable
===============================================================================*/
/*==============================================================================
Function Implementation
===============================================================================*/

esp_err_t start_test(tests_t test)
{
    xTaskCreate(tests_task, "tests_task", 4 * 1024, (void *)test, 1, NULL);
    return ESP_OK;
}

static void tests_task(void *pvParameters)
{
    tests_t test = (tests_t)pvParameters;
    ESP_LOGI(TAG, "Tests %s started", tests_available_tests[test]);
    while (1)
    {
        switch (test)
        {
        case TEST_ALL:
            adc_test();
            break;
        case TEST_ADC:
            adc_test();
            break;
        default:
            break;
        }

        vTaskDelay(1000 / portTICK_PERIOD_MS);
    }
}
static esp_err_t adc_test()
{
    ESP_LOGI(TAG, "VCondo: %f", gpio_get_vcondo());
    ESP_LOGI(TAG, "VUSB: %f", gpio_get_vusb());
    return ESP_OK;
}