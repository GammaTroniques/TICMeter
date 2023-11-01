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

/*==============================================================================
 Public Defines
==============================================================================*/
#define AP_SSID "TICMeter"
#define AP_PASS ""
#define HOSTNAME "TICMeter"

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
    char host[100];
    uint16_t port;
    char username[100];
    char password[100];
    char topic[100];
} mqttConfig_t;

typedef struct
{
    char productID[30];
    char deviceUUID[30];
    char deviceAuth[40];
} tuyaConfig_t;

typedef struct
{
    char ssid[50];
    char password[50];

    linky_mode_t linkyMode;
    connectivity_t mode;
    webConfig_t web;
    mqttConfig_t mqtt;
    tuyaConfig_t tuyaKeys;
    uint8_t tuyaBinded;

    char version[10];
    uint16_t refreshRate;
    uint8_t sleep;
} config_t;

/*==============================================================================
 Public Variables Declaration
==============================================================================*/
extern const char *MODES[];
extern const char *GIT_TAG;
extern const char *GIT_REV;
extern const char *GIT_BRANCH;
extern const char *BUILD_TIME;

extern config_t config_values;
/*==============================================================================
 Public Functions Declaration
==============================================================================*/

int8_t config_erase();
int8_t config_begin();
int8_t config_read();
int8_t config_write();
uint8_t config_verify();

int16_t config_calculate_checksum();
#endif /* CONFIG_H */