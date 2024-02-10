/**
 * @file
 * @author Dorian Benech
 * @brief
 * @version 1.0
 * @date 2023-10-19
 *
 * @copyright Copyright (c) 2023 GammaTroniques
 *
 */

#ifndef ZIGBEE_H
#define ZIGBEE_H

/*==============================================================================
 Local Include
===============================================================================*/
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include "ha/esp_zigbee_ha_standard.h"
#include "ha/zb_ha_device_config.h"
#include "zcl/esp_zigbee_zcl_power_config.h"
#include "linky.h"
/*==============================================================================
 Public Defines
==============================================================================*/

#define LINKY_TIC_ENDPOINT 1
#define TICMETER_CLUSTER_ID 0xFF42
#define ZIGBEE_CHANNEL_MASK (1l << 15)

/*==============================================================================
 Public Macro
==============================================================================*/

/*==============================================================================
 Public Type
==============================================================================*/

/*==============================================================================
 Public Variables Declaration
==============================================================================*/

/*==============================================================================
 Public Functions Declaration
==============================================================================*/
extern void zigbee_init_stack();
extern uint8_t zigbee_send(linky_data_t *data);
void zigbee_start_pairing();
uint8_t zigbee_factory_reset();
#endif // ZIGBEE_H