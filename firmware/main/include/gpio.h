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
#include "esp_adc/adc_oneshot.h"

/*==============================================================================
 Public Defines
==============================================================================*/
#define TICMETER_HW_VERSION "3.2"

#define RX_LINKY (gpio_num_t)23
#define V_CONDO_PIN ADC_CHANNEL_4
#define V_USB_PIN ADC_CHANNEL_1
#define PAIRING_PIN (gpio_num_t)3
// #define PAIRING_PIN (gpio_num_t)9
#define BOOT_PIN (gpio_num_t)9
#define RESET_PIN (gpio_num_t)15

#define LED_EN (gpio_num_t)0
#define LED_DATA (gpio_num_t)5

#define PAIRING_LED_PIN (gpio_num_t)23 // 23 --> unused
#define LED_RED (gpio_num_t)23
#define LED_GREEN (gpio_num_t)23

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
extern TaskHandle_t gpio_led_pairing_task_handle;
extern uint32_t gpio_start_push_time;
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
 * @brief Get the state of the USB
 *
 * @return uint8_t: 1 if connected, 0 if not
 */
uint8_t gpio_vusb_connected();

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

void gpio_peripheral_reinit();

void gpio_start_pariring();

void gpio_restart_in_pairing();

#endif /* __GPIO_H__ */