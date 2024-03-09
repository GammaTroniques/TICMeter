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

#ifndef TESTS_H
#define TESTS_H

/*==============================================================================
 Local Include
===============================================================================*/
#include "esp_ota_ops.h"
#include "linky.h"

/*==============================================================================
 Public Defines
==============================================================================*/

/*==============================================================================
 Public Macro
==============================================================================*/

/*==============================================================================
 Public Type
==============================================================================*/
typedef enum
{
    TEST_ALL,
    TEST_ADC,
    TEST_LINKY_HIST,
    TEST_LINKY_STD,
} tests_t;

/*==============================================================================
 Public Variables Declaration
==============================================================================*/
extern esp_err_t (*tests_available_tests[])(void *ptr);
extern const char *const tests_str_available_tests[];

extern const uint32_t tests_count;

extern const linky_data_hist tests_hist_data;
extern const linky_data_std tests_std_data;
/*==============================================================================
 Public Functions Declaration
==============================================================================*/
esp_err_t start_test(tests_t test);

#endif /* TESTS_H */