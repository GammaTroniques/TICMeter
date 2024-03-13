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
#define WIFI_CONNECT_TIMEOUT 15000
/*==============================================================================
 Public Macro
==============================================================================*/

/*==============================================================================
 Public Type
==============================================================================*/
typedef enum
{
    WIFI_DISCONNECTED,
    WIFI_STARTED,
    WIFI_CONNECTING,
    WIFI_CONNECTED,
    WIFI_FAILED,
} wifi_state_t;

/*==============================================================================
 Public Variables Declaration
==============================================================================*/
extern wifi_state_t wifi_state;
extern wifi_ap_record_t wifi_ap_list[20];
extern esp_netif_ip_info_t wifi_current_ip;
/*==============================================================================
 Public Functions Declaration
==============================================================================*/

extern uint8_t wifi_init();

/**
 * @brief connect to wifi
 *
 * @return 0 if success, else esp_err_t
 */
extern esp_err_t wifi_connect();

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
extern void wifi_http_get_config_from_server();

extern void wifi_scan(uint16_t *ap_count);

extern esp_err_t wifi_ping(ip_addr_t host, uint32_t *ping_time);

#endif /* WIFI_H */
