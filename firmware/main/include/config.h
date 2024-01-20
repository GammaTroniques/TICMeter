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

/*==============================================================================
 Public Defines
==============================================================================*/
#define AP_SSID "TICMeter"
#define AP_PASS ""
#define HOSTNAME "TICMeter"

// smaller number = smaller priority

#define PRIORITY_TEST 1
#define PRIORITY_ZIGBEE 3
#define PRIORITY_TUYA 3
#define PRIORITY_SHELL 5
#define PRIORITY_OTA 10
#define PRIORITY_MQTT 5
#define PRIORITY_FETCH_LINKY 1
#define PRIORITY_PAIRING 1
#define PRIORITY_DNS 16

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
    MODE_WEB,
    MODE_MQTT,
    MODE_MQTT_HA,
    MODE_ZIGBEE,
    MODE_MATTER,
    MODE_TUYA,
} connectivity_t;

typedef struct
{
    char host[100];
    char postUrl[50];
    char configUrl[50];
    char token[100];
} webConfig_t;

typedef struct
{
    char host[101];
    uint16_t port;
    char username[51];
    char password[51];
    char topic[101];
} mqttConfig_t;

typedef enum
{
    TUYA_NOT_CONFIGURED,
    TUYA_BLE_PAIRING,
    TUYA_WIFI_CONNECTING,
    TUYA_PAIRED,
} pairing_state_t;

typedef struct
{
    char product_id[30];
    char device_uuid[30];
    char device_auth[40];
} tuyaConfig_t;

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

} zigbee_config_t;

typedef struct
{
    uint8_t initialized; // should be 1 if the config is initialized
    char ssid[50];
    char password[50];

    linky_mode_t linkyMode;
    linky_mode_t last_linky_mode;
    connectivity_t mode;
    webConfig_t web;
    mqttConfig_t mqtt;
    pairing_state_t pairing_state;
    tuyaConfig_t tuya;
    zigbee_config_t zigbee;

    char version[10];
    uint16_t refreshRate;
    uint8_t sleep;
} config_t;

typedef struct
{
    char serialNumber[13];
    char macAddress[13];
} efuse_t;

/*==============================================================================
 Public Variables Declaration
==============================================================================*/
extern const char *MODES[];
extern const char *GIT_TAG;
extern const char *GIT_REV;
extern const char *GIT_BRANCH;
extern const char *BUILD_TIME;

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
uint8_t config_efuse_write(const char *serialnumber, uint8_t len);
#endif /* CONFIG_H */