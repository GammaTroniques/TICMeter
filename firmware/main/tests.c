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
#include "config.h"
#include "esp_log.h"
#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/event_groups.h"
#include "driver/gpio.h"
#include "gpio.h"
#include "driver/uart.h"
#include "linky.h"
#include "common.h"
#include "wifi.h"
#include "main.h"
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
static esp_err_t test_adc(void *ptr);
static esp_err_t test_linky_hist(void *ptr);
static esp_err_t test_linky_std(void *ptr);
static void tests_task(void *pvParameters);

/*==============================================================================
Public Variable
===============================================================================*/
esp_err_t (*tests_available_tests[])(void *ptr) = {
    [TEST_ALL] = test_adc,
    [TEST_ADC] = test_adc,
    [TEST_LINKY_HIST] = test_linky_hist,
    [TEST_LINKY_STD] = test_linky_std,
};

const char *const tests_str_available_tests[] = {
    [TEST_ALL] = "all",
    [TEST_ADC] = "adc",
    [TEST_LINKY_HIST] = "linky-hist",
    [TEST_LINKY_STD] = "linky-std",
};

const uint32_t tests_count = sizeof(tests_str_available_tests) / sizeof(char *);
/*==============================================================================
 Local Variable
===============================================================================*/
/*==============================================================================
Function Implementation
===============================================================================*/

esp_err_t start_test(tests_t test)
{
    xTaskCreate(tests_task, "tests_task", 16 * 1024, (void *)test, PRIORITY_TEST, NULL);
    return ESP_OK;
}

static void tests_task(void *pvParameters)
{
    suspendTask(main_task_handle);
    tests_t test = (tests_t)pvParameters;
    ESP_LOGI(TAG, "Tests %s started", tests_str_available_tests[test]);

    if (tests_available_tests[test](NULL) == ESP_OK)
    {
        ESP_LOGI(TAG, "Tests %s passed", tests_str_available_tests[test]);
    }
    else
    {
        ESP_LOGE(TAG, "Tests %s failed", tests_str_available_tests[test]);
    }

    vTaskDelete(NULL);
}
static esp_err_t test_adc(void *ptr)
{
    ESP_LOGI(TAG, "VCondo: %f", gpio_get_vcondo());
    ESP_LOGI(TAG, "VUSB: %f", gpio_get_vusb());
    return ESP_OK;
}

static esp_err_t test_linky_hist(void *ptr)
{
    linky_data_hist data = {
        .ADCO = "123456789012",
        .OPTARIF = "BASE",
        .ISOUSC = 32,
        .BASE = 123456,
        .HCHC = 223456,
        .HCHP = 323456,
        .EJPHN = 423456,
        .EJPHPM = 523456,
        .BBRHCJB = 623456,
        .BBRHPJB = 723456,
        .BBRHCJW = 823456,
        .BBRHPJW = 923456,
        .BBRHCJR = 1023456,
        .BBRHPJR = 1123456,
        .PEJP = 0,
        .PTEC = "TH..",
        .DEMAIN = "----",
        .IINST = 13,
        .IINST1 = 14,
        .IINST2 = 15,
        .IINST3 = 16,
        .IMAX = 17,
        .IMAX1 = 18,
        .IMAX2 = 19,
        .IMAX3 = 20,
        .ADPS = 21,
        .ADIR1 = 22,
        .ADIR2 = 23,
        .ADIR3 = 24,
        .PAPP = 26,
        .PMAX = 27,
        .PPOT = 28,
        .HHPHC = "A",
        .MOTDETAT = "000000",
    };
    linky_data.hist = data;
    main_send_data();
    return ESP_OK;
}

static esp_err_t test_linky_std(void *ptr)
{
    linky_data_std data = {
        .ADSC = "123456789012",
        .VTIC = "2",
        .DATE = {.value = 0, .timestamp = wifi_get_timestamp()},
        .NGTF = "TEMPO",
        .LTARF = "HP  BLEU",
        .EAST = 50019226,
        .EASF01 = 022235340,
        .EASF02 = 26587270,
        .EASF03 = 26587270,
        .EASF04 = 494614,
        .EASF05 = 115045,
        .EASF06 = 161261,
        .EASF07 = 7,
        .EASF08 = 8,
        .EASF09 = 9,
        .EASF10 = 10,
        .EASD01 = 11,
        .EASD02 = 12,
        .EASD03 = 13,
        .EASD04 = 14,
        .EAIT = 115,
        .IRMS1 = 15,
        .IRMS2 = 16,
        .IRMS3 = 17,
        .URMS1 = 18,
        .URMS2 = 19,
        .URMS3 = 20,
        .PREF = 21,
        .PCOUP = 22,
        .SINSTS = 23,
        .SINSTS1 = 24,
        .SINSTS2 = 25,
        .SINSTS3 = 26,
        .SMAXSN = {.value = 27, .timestamp = wifi_get_timestamp()},
        .SMAXSN1 = {.value = 28, .timestamp = wifi_get_timestamp()},
        .SMAXSN2 = {.value = 29, .timestamp = wifi_get_timestamp()},
        .SMAXSN3 = {.value = 30, .timestamp = wifi_get_timestamp()},

        .SMAXSN_1 = {.value = 31, .timestamp = wifi_get_timestamp()},
        .SMAXSN1_1 = {.value = 32, .timestamp = wifi_get_timestamp()},
        .SMAXSN2_1 = {.value = 33, .timestamp = wifi_get_timestamp()},
        .SMAXSN3_1 = {.value = 34, .timestamp = wifi_get_timestamp()},

        .SINSTI = 35,

        .SMAXIN = {.value = 36, .timestamp = wifi_get_timestamp()},
        .SMAXIN_1 = {.value = 37, .timestamp = wifi_get_timestamp()},

        .CCASN = {.value = 38, .timestamp = wifi_get_timestamp()},
        .CCASN_1 = {.value = 39, .timestamp = wifi_get_timestamp()},
        .CCAIN = {.value = 40, .timestamp = wifi_get_timestamp()},
        .CCAIN_1 = {.value = 41, .timestamp = wifi_get_timestamp()},

        .UMOY1 = {.value = 42, .timestamp = wifi_get_timestamp()},
        .UMOY2 = {.value = 43, .timestamp = wifi_get_timestamp()},
        .UMOY3 = {.value = 44, .timestamp = wifi_get_timestamp()},

        .STGE = "000000",

        .DPM1 = {.value = 45, .timestamp = wifi_get_timestamp()},
        .FPM1 = {.value = 46, .timestamp = wifi_get_timestamp()},
        .DPM2 = {.value = 47, .timestamp = wifi_get_timestamp()},
        .FPM2 = {.value = 48, .timestamp = wifi_get_timestamp()},
        .DPM3 = {.value = 49, .timestamp = wifi_get_timestamp()},
        .FPM3 = {.value = 50, .timestamp = wifi_get_timestamp()},

        .MSG1 = "test",
        .MSG2 = "un test",
        .PRM = "25555555926695",
        .RELAIS = "00000",
        .NTARF = 2,
        .NJOURF = 0,
        .NJOURF_1 = 2,
        .PJOURF_1 = "00",
        .PPOINTE = "000000",
    };

    linky_set_mode(MODE_STD);
    linky_data.std = data;
    linky_print();
    main_send_data();

    return ESP_OK;
}