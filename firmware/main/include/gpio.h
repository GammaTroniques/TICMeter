/**
 * @file gpio.h
 * @author Dorian Benech
 * @brief
 * @version 1.0
 * @date 2023-10-11
 *
 * @copyright Copyright (c) 2023 GammaTroniques
 *
 */

#ifndef __GPIO_H__
#define __GPIO_H__

/*==============================================================================
 Local Include
===============================================================================*/
#include <stdio.h>

/*==============================================================================
 Public Defines
==============================================================================*/
#define RX_LINKY (gpio_num_t)23
#define V_CONDO_PIN ADC_CHANNEL_4
#define V_USB_PIN 1 // ADC_CHANNEL_1
#define PAIRING_PIN (gpio_num_t)3
// #define PAIRING_PIN (gpio_num_t)9
#define BOOT_PIN (gpio_num_t)9

#define LED_EN (gpio_num_t)0
#define LED_DATA (gpio_num_t)5

#define PAIRING_LED_PIN (gpio_num_t)23 // 23 --> unused
#define LED_RED (gpio_num_t)23
#define LED_GREEN (gpio_num_t)23

#define PATTERN_WIFI_CONNECTING 0
#define PATTERN_WIFI_RETRY 1
#define PATTERN_WIFI_FAILED 2
#define PATTERN_LINKY_OK 3
#define PATTERN_LINKY_ERR 4
#define PATTERN_SEND_OK 5
#define PATTERN_SEND_ERR 6
#define PATTERN_NO_CONFIG 7
#define PATTERN_START 8
#define PATTERN_PAIRING 9

/*==============================================================================
 Public Macro
==============================================================================*/

/*==============================================================================
 Public Type
==============================================================================*/

/*==============================================================================
 Public Variables Declaration
==============================================================================*/
extern TaskHandle_t gpip_led_ota_task_handle;

/*==============================================================================
 Public Functions Declaration
==============================================================================*/

/**
 * @brief Init the GPIOs
 *
 */
void gpio_init_pins();

/**
 * @brief  Get the tension of the USB
 *
 * @return float: voltage in V
 */
float gpio_get_vusb();

/**
 * @brief Get the tension of the condo
 *
 * @return float: voltage in V
 */
float gpio_get_vcondo();

/**
 * @brief The pairing button task
 *
 * @param pvParameter Not used
 */
void gpio_pairing_button_task(void *pvParameter);

/**
 * @brief Start a led pattern
 *
 * @param pattern: see ledPattern
 */
void gpio_start_led_pattern(uint8_t pattern);

/**
 * @brief The blink led task when no config is found
 *
 * @param pvParameters Not used
 */
void gpio_led_task_no_config(void *pvParameters);

/**
 * @brief The blink led task when wifi is connecting
 *
 * @param pvParameters Not used
 */
void gpio_led_task_wifi_connecting(void *pvParameters);

/**
 * @brief The blink led task when linky data is fetching
 *
 * @param pvParameters Not used
 */
void gpio_led_task_linky_reading(void *pvParameters);

/**
 * @brief The blink led task when data is sending
 *
 * @param pvParameters Not used
 */
void gpio_led_task_sending(void *pvParameters);

/**
 * @brief The blink led task when pairing is in progress
 *
 * @param pvParameters Not used
 */
void gpio_led_task_pairing(void *pvParameters);

/**
 * @brief Start a led pattern: Blink led when starting with the color of the mode
 *
 */
void gpio_boot_led_pattern();

void gpio_led_task_ota(void *pvParameters);

#endif /* __GPIO_H__ */