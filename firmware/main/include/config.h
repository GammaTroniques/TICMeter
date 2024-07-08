/**
 * @file config.h
 * @author Dorian Benech
 * @brief
 * @version 1.0
 * @date 2023-10-11
 *
 * @copyright Copyright (c) 2023 GammaTroniques
 *
 */

#ifndef CONFIG_H
#define CONFIG_H

/*==============================================================================
 Local Include
===============================================================================*/

#include <stdio.h>
#include <inttypes.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_system.h"
#include "nvs_flash.h"
#include "nvs.h"
#include "esp_log.h"
#include "linky.h"
#include "common.h"
#include "efuse_table.h"
#include "version.h"
#include <assert.h>
/*==============================================================================
 Public Defines
==============================================================================*/
#define AP_SSID "TICMeter"
#define AP_PASS ""
#define HOSTNAME "TICMeter"

// smaller number = smaller priority

#define PRIORITY_TEST 1
#define PRIORITY_ZIGBEE 5 // old 3
#define PRIORITY_TUYA 3
#define PRIORITY_SHELL 5
#define PRIORITY_OTA 10
#define PRIORITY_MQTT 5
#define PRIORITY_FETCH_LINKY 1
#define PRIORITY_PAIRING 1
#define PRIORITY_DNS 16

#define PRIORITY_LED 5
#define PRIORITY_LED_PATTERN 5
#define PRIORITY_LED_PAIRING 5
#define PRIORITY_LED_WIFI 1
#define PRIORITY_LED_SENDING 1
#define PRIORITY_LED_NO_CONFIG 1
#define PRIORITY_LED_LINKY_READING 10
#define PRIORITY_STOP_CAPTIVE_PORTAL 5

/*==============================================================================
 Public Macro
==============================================================================*/
#define MILLIS xTaskGetTickCount() * portTICK_PERIOD_MS
/*==============================================================================
 Public Type
==============================================================================*/
typedef enum
{
    MODE_NONE,
    MODE_HTTP,
    MODE_MQTT,
    MODE_MQTT_HA,
    MODE_ZIGBEE,
    MODE_TUYA,
    MODE_LAST,
    // later
    MODE_MATTER,
} connectivity_t;

typedef struct
{
    char host[100];
    char postUrl[50];
    char configUrl[50];
    char token[100];
    uint8_t store_before_send;
} web_config_t;

typedef struct
{
    char host[101];
    uint16_t port;
    char username[51];
    char password[70];
    char topic[101];
} mqtt_config_t;

typedef enum
{
    TUYA_NOT_CONFIGURED,
    TUYA_BLE_PAIRING,
    TUYA_WIFI_CONNECTING,
    TUYA_PAIRED,
} pairing_state_t;

typedef struct
{
    char device_uuid[30];
    char device_auth[40];
} tuya_config_t;

typedef enum
{
    ZIGBEE_NOT_CONFIGURED,
    ZIGBEE_PAIRING,
    ZIGBEE_PAIRED,
    ZIGBEE_WANT_PAIRING,
} zigbee_pairing_state_t;

typedef struct
{
    zigbee_pairing_state_t state;
    uint32_t last_attribute_count;
    uint8_t spare[8];

} zigbee_config_t;

typedef struct
{
    uint64_t index_total;
    uint64_t index_hc;
    uint64_t index_hp;
    uint64_t index_production;
    uint8_t value_saved;
} index_offset_t;

typedef struct
{
    uint8_t initialized; // should be 1 if the config is initialized
    char ssid[33];
    char password[65];

    linky_mode_t linky_mode;
    linky_mode_t last_linky_mode;
    connectivity_t mode;
    web_config_t web;
    mqtt_config_t mqtt;
    pairing_state_t pairing_state;
    tuya_config_t tuya;
    zigbee_config_t zigbee;

    char version[10];
    uint16_t refresh_rate;
    uint8_t sleep;
    index_offset_t index_offset;
    uint8_t boot_pairing;
} config_t;

typedef struct
{
    char serial_number[13];
    char mac_address[13];
    char hw_version[3];
} efuse_t;

#define HW_VERSION_CHECK(MAJOR, MINOR, PATCH) (efuse_values.hw_version[0] == MAJOR && efuse_values.hw_version[1] == MINOR && efuse_values.hw_version[2] == PATCH)

/*==============================================================================
 Public Variables Declaration
==============================================================================*/
extern const char *const MODES[];

#ifdef GIT_TAG
#define PRODUCTION 1
#endif

extern config_t config_values;
extern efuse_t efuse_values;
/*==============================================================================
 Public Functions Declaration
==============================================================================*/

int8_t config_erase();
int8_t config_begin();
int8_t config_read();
int8_t config_write();
uint8_t config_verify();
uint8_t config_rw();
uint8_t config_efuse_read();
uint8_t config_efuse_write(const char *serialnumber, uint8_t len, const uint8_t *hw_version);
uint8_t config_factory_reset();
esp_err_t config_erase_partition(const char *partition_label);
uint32_t config_get_hw_version();
const char *config_get_str_mode();

#endif /* CONFIG_H */