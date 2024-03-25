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
static esp_err_t test_mode(void *ptr);
static esp_err_t test_linky_hist(void *ptr);
static esp_err_t test_linky_std(void *ptr);
static esp_err_t test_linky_read(void *ptr);
static esp_err_t test_linky_stats(void *ptr);
static void tests_task(void *pvParameters);

/*==============================================================================
Public Variable
===============================================================================*/
esp_err_t (*tests_available_tests[])(void *ptr) = {
    [TEST_MODE] = test_mode,
    [TEST_ALL] = test_adc,
    [TEST_ADC] = test_adc,
    [TEST_LINKY_HIST] = test_linky_hist,
    [TEST_LINKY_STD] = test_linky_std,
    [TEST_LINKY_READ] = test_linky_read,
    [TEST_LINKY_STATS] = test_linky_stats,
};

const char *const tests_str_available_tests[] = {
    [TEST_MODE] = "mode",
    [TEST_ALL] = "all",
    [TEST_ADC] = "adc",
    [TEST_LINKY_HIST] = "linky-hist",
    [TEST_LINKY_STD] = "linky-std",
    [TEST_LINKY_READ] = "linky-read",
    [TEST_LINKY_STATS] = "linky-stats",
};

const uint32_t tests_count = sizeof(tests_str_available_tests) / sizeof(char *);
const linky_data_std tests_std_data = {
    .ADSC = "123456789012",
    .VTIC = "2",
    .DATE = {.value = 0, .time = 1710017481},
    .NGTF = "TEMPO",
    .LTARF = "HP  BLEU",
    .EAST = 50019226,
    .EASF01 = 022235340,
    .EASF02 = 26587280,
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
    .SMAXSN = {.value = 27, .time = 1710017481},
    .SMAXSN1 = {.value = 28, .time = 1710017481},
    .SMAXSN2 = {.value = 29, .time = 1710017481},
    .SMAXSN3 = {.value = 30, .time = 1710017481},

    .SMAXSN_1 = {.value = 31, .time = 1710017481},
    .SMAXSN1_1 = {.value = 32, .time = 1710017481},
    .SMAXSN2_1 = {.value = 33, .time = 1710017481},
    .SMAXSN3_1 = {.value = 34, .time = 1710017481},

    .SINSTI = 35,

    .SMAXIN = {.value = 36, .time = 1710017481},
    .SMAXIN_1 = {.value = 37, .time = 1710017481},

    .CCASN = {.value = 38, .time = 1710017481},
    .CCASN_1 = {.value = 39, .time = 1710017481},
    .CCAIN = {.value = 40, .time = 1710017481},
    .CCAIN_1 = {.value = 41, .time = 1710017481},

    .UMOY1 = {.value = 42, .time = 1710017481},
    .UMOY2 = {.value = 43, .time = 1710017481},
    .UMOY3 = {.value = 44, .time = 1710017481},

    .STGE = "000000",

    .DPM1 = {.value = 45, .time = 1710017481},
    .FPM1 = {.value = 46, .time = 1710017481},
    .DPM2 = {.value = 47, .time = 1710017481},
    .FPM2 = {.value = 48, .time = 1710017481},
    .DPM3 = {.value = 49, .time = 1710017481},
    .FPM3 = {.value = 50, .time = 1710017481},

    .MSG1 = "test",
    .MSG2 = "un test",
    .PRM = "25555555926695",
    .RELAIS = "00000",
    .NTARF = 2,
    .NJOURF = 0,
    .NJOURF_1 = 2,
    .PJOURF_1 = "00",
    .PPOINTE = "000000",

    .ERQ1 = 51,
    .ERQ2 = 52,
    .ERQ3 = 53,
    .ERQ4 = 54,

};

const linky_data_hist tests_hist_data = {
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
    printf("\x02Tests %s started\n", tests_str_available_tests[test]);

    if (tests_available_tests[test](NULL) == ESP_OK)
    {
        printf("Tests %s passed\n", tests_str_available_tests[test]);
    }
    else
    {
        printf("Tests %s failed\n", tests_str_available_tests[test]);
    }
    printf("\x03");
    vTaskDelete(NULL);
}
static esp_err_t test_adc(void *ptr)
{
    printf("Condo: %f\n", gpio_get_vcondo());
    printf("VUSB: %f\n", gpio_get_vusb());
    return ESP_OK;
}

static esp_err_t test_linky_hist(void *ptr)
{

    linky_data.hist = tests_hist_data;
    main_send_data();
    return ESP_OK;
}

static esp_err_t test_linky_std(void *ptr)
{

    linky_set_mode(MODE_STD);
    linky_data.std = tests_std_data;
    linky_print();
    // main_send_data();

    return ESP_OK;
}

static esp_err_t test_mode(void *ptr)
{
    printf("Start test mode\n");
    suspendTask(main_task_handle);
    return ESP_OK;
}

static esp_err_t test_linky_read(void *ptr)
{
    linky_set_mode(MODE_HIST);
    if (!linky_update())
    {
        printf("Linky update failed\n");
        return ESP_FAIL;
    }
    printf("Linky update success\n");
    return ESP_OK;
}

static esp_err_t test_linky_stats(void *ptr)
{
    linky_stats();
    return ESP_OK;
}
