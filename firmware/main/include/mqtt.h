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
extern int mqtt_app_start();

/**
 * @brief Start this task to stop the MQTT client (take some times (1-3s))
 *
 */
extern void mqtt_stop_task(void *pvParameter);

/**
 * @brief Send all entities to Home Assistant
 *
 */
extern void mqtt_setup_ha_discovery();

/**
 * @brief Send LinkyData to MQTT
 *
 * @param linky
 */
extern int mqtt_send(LinkyData *linky);

#endif /* MQTT_H */
