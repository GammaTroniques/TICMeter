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

// #define EXAMPLE_ESP_WIFI_SSID "wifirobot"
// #define EXAMPLE_ESP_WIFI_PASS "robot2004LARIS"

#define EXAMPLE_ESP_WIFI_SSID "AFDD"
#define EXAMPLE_ESP_WIFI_PASS "afddChromino"

#define EXAMPLE_ESP_MAXIMUM_RETRY 3
#pragma GCC diagnostic ignored "-Wmissing-field-initializers"
/**
 * @brief set the CPU frequency to 240Mhz and connect to wifi
 *
 * @return 1 if connected, 0 if not
 */
uint8_t connectToWifi();

void disconectFromWifi();

static void event_handler(void *arg, esp_event_base_t event_base,
                          int32_t event_id, void *event_data);

/* FreeRTOS event group to signal when we are connected*/
static EventGroupHandle_t s_wifi_event_group;

/* The event group allows multiple bits for each event, but we only care about two events:
 * - we are connected to the AP with an IP
 * - we failed to connect after the maximum amount of retries */
#define WIFI_CONNECTED_BIT BIT0
#define WIFI_FAIL_BIT BIT1

static const char *TAG = "wifi station";

static int s_retry_num = 0;

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
char sendToServer(char *json, Config *config);
#endif