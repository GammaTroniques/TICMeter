#ifndef WIFI_H
#define WIFI_H

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

#define ESP_MAXIMUM_RETRY 5
#define WIFI_CONNECT_TIMEOUT 10000
// #pragma GCC diagnostic ignored "-Wmissing-field-initializers"
/**
 * @brief set the CPU frequency to 240Mhz and connect to wifi
 *
 * @return 1 if connected, 0 if not
 */
uint8_t connectToWifi();

void disconectFromWifi();

void event_handler(void *arg, esp_event_base_t event_base,
                   int32_t event_id, void *event_data);

/* The event group allows multiple bits for each event, but we only care about two events:
 * - we are connected to the AP with an IP
 * - we failed to connect after the maximum amount of retries */
#define WIFI_CONNECTED_BIT BIT0
#define WIFI_FAIL_BIT BIT1

/**
 * @brief Create a Http Url (http://host/path)
 *
 * @param url the destination url
 * @param host the host
 * @param path the path
 */
void createHttpUrl(char *url, const char *host, const char *path);

/**
 * @brief Get config from server and save it in EEPROM
 *
 */
void getConfigFromServer(Config *config);

/**
 * @brief Get the Timestamp in seconds
 *
 * @return timestamp
 */
time_t getTimestamp();

esp_err_t send_data_handler(esp_http_client_event_handle_t evt);

/**
 * @brief Send json data to server
 *
 * @param json the json data to send
 * @return the http code
 */
uint8_t sendToServer(const char *json);

extern uint8_t wifiConnected;
extern uint8_t sendingValues;

void start_captive_portal();

uint8_t reconnectToWifi();

uint8_t sntpInit();
#endif