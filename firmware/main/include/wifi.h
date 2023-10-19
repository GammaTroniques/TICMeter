/**
 * @file
 * @author Dorian Benech
 * @brief
 * @version 1.0
 * @date 2023-10-11
 *
 * @copyright Copyright (c) 2023 GammaTroniques
 *
 */

#ifndef WIFI_H
#define WIFI_H

/*==============================================================================
 Local Include
===============================================================================*/
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/event_groups.h"
#include "esp_system.h"
#include "esp_wifi.h"
#include "esp_event.h"
#include "esp_log.h"
#include "nvs_flash.h"
#include "esp_sntp.h"

#include "esp_http_client.h"

#include "lwip/err.h"
#include "lwip/sys.h"

#include "config.h"

/*==============================================================================
 Public Defines
==============================================================================*/
#define WIFI_CONNECT_TIMEOUT 10000
/*==============================================================================
 Public Macro
==============================================================================*/

/*==============================================================================
 Public Type
==============================================================================*/

/*==============================================================================
 Public Variables Declaration
==============================================================================*/
extern uint8_t wifi_connected;
extern uint8_t wifi_sending;
/*==============================================================================
 Public Functions Declaration
==============================================================================*/

/**
 * @brief connect to wifi
 *
 * @return 1 if connected, 0 if not
 */
extern uint8_t wifi_connect();

/**
 * @brief disconnect from wifi
 *
 */
extern void wifi_disconnect();

/**
 * @brief Get the Timestamp in seconds
 *
 * @return timestamp
 */
extern time_t wifi_get_timestamp();

/**
 * @brief Send json data to server
 *
 * @param json the json data to send
 * @return the http code
 */
extern uint8_t wifi_send_to_server(const char *json);

/**
 * @brief Start the captive portal
 *
 */
extern void wifi_start_captive_portal();

/**
 * @brief Get the config from server
 *
 * @param config
 */
extern void wifi_http_get_config_from_server(Config *config);

#endif /* WIFI_H */
