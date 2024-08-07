/**
 * @file mqtt.h
 * @author Dorian Benech
 * @brief
 * @version 1.0
 * @date 2023-10-11
 *
 * @copyright Copyright (c) 2023 GammaTroniques
 *
 */

#ifndef MQTT_H
#define MQTT_H

/*==============================================================================
 Local Include
===============================================================================*/
#include "config.h"
#include <stdio.h>
#include "esp_log.h"
#include "mqtt_client.h"
#include "esp_event.h"

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"
#include "freertos/queue.h"

#include "esp_log.h"
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
extern esp_mqtt_client_handle_t mqtt_client;

/*==============================================================================
 Public Functions Declaration
==============================================================================*/

/**
 * @brief The MQTT event handler
 *
 * @param handler_args
 * @param base
 * @param event_id
 * @param event_data
 */
extern void mqtt_event_handler(void *handler_args, esp_event_base_t base, int32_t event_id, void *event_data);

/**
 * @brief Start the MQTT client
 *
 */
extern int mqtt_init();

extern int mqtt_deinit();

/**
 * @brief Send all entities to Home Assistant
 *
 */
extern void mqtt_setup_ha_discovery(bool with_delete);

/**
 * @brief Send linky_data_t to MQTT
 *
 * @param linky
 */
extern int mqtt_send();

extern uint8_t mqtt_prepare_publish(linky_data_t *linky);

esp_err_t mqtt_test(esp_mqtt_error_type_t *type, esp_mqtt_connect_return_code_t *return_code);

#endif /* MQTT_H */
