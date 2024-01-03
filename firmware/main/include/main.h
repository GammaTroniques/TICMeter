/**
 * @file main.h
 * @author Dorian Benech
 * @brief
 * @version 1.0
 * @date 2023-10-11
 *
 * @copyright Copyright (c) 2023 GammaTroniques
 *
 */

#ifndef MAIN_H
#define MAIN_H

/*==============================================================================
 Local Include
===============================================================================*/
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

/*==============================================================================
 Public Variables Declaration
==============================================================================*/
extern TaskHandle_t fetchLinkyDataTaskHandle;
extern TaskHandle_t sendDataTaskHandle;
extern TaskHandle_t noConfigLedTaskHandle;
extern uint32_t main_sleep_time;

/*==============================================================================
 Public Functions Declaration
==============================================================================*/

/**
 * @brief The Linky data fetch task
 *
 * @param pvParameters Not used
 */
void main_fetch_linky_data_task(void *pvParameters);

#endif /* MAIN_H */