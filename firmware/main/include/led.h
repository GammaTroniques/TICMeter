/**
 * @file led.c
 * @author Dorian Benech
 * @brief
 * @version 1.0
 * @date 2023-10-11
 *
 * @copyright Copyright (c) 2023 GammaTroniques
 *
 */

#ifndef LED_H
#define LED_H

/*==============================================================================
 Local Include
===============================================================================*/
#include <stdio.h>
#include <stdint.h>

/*==============================================================================
 Public Defines
==============================================================================*/

/*==============================================================================
 Public Macro
==============================================================================*/

typedef enum
{
    LED_BOOT,
    LED_NO_CONFIG,
    LED_FACTORY_RESET,

    LED_LINKY_READING,
    LED_LINKY_FAILED,

    LED_CONNECTING,
    LED_CONNECTING_FAILED,

    LED_SEND_OK,
    LED_SENDING,
    LED_SEND_FAILED,

    LED_PAIRING,

    LED_OTA_AVAILABLE,
    LED_OTA_IN_PROGRESS,

} led_pattern_t;

/*==============================================================================
 Public Type
==============================================================================*/

/*==============================================================================
 Public Variables Declaration
==============================================================================*/

/*==============================================================================
 Public Functions Declaration
==============================================================================*/

uint32_t led_init();

void led_start_pattern(led_pattern_t pattern);

void led_stop_pattern(led_pattern_t pattern);

#endif /* LED_H */