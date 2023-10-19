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
#define AP_SSID "Linky 5G Pfizer#9432"
#define AP_PASS ""
#define HOSTNAME "Linky"

/*==============================================================================
 Public Macro
==============================================================================*/
#define MILLIS xTaskGetTickCount() * portTICK_PERIOD_MS
/*==============================================================================
 Public Type
==============================================================================*/
enum connectivity_t : uint8_t
{
    MODE_WEB,
    MODE_MQTT,
    MODE_MQTT_HA,
    MODE_ZIGBEE,
    MODE_MATTER,
    MODE_TUYA,
};

struct webConfig_t
{
    char host[100] = "";
    char postUrl[50] = "";
    char configUrl[50] = "";
    char token[100] = "";
};

struct mqttConfig_t
{
    char host[100] = "";
    uint16_t port = 0;
    char username[100] = "";
    char password[100] = "";
    char topic[100] = "";
};

struct tuyaConfig_t
{
    char productID[30] = "";
    char deviceUUID[30] = "";
    char deviceAuth[40] = "";
};

struct config_t
{
    uint16_t magic = 0x1234; // magic number to check if a config is stored in EEPROM
    char ssid[50] = "";
    char password[50] = "";

    LinkyMode linkyMode = MODE_HISTORIQUE;
    connectivity_t mode = MODE_WEB;
    webConfig_t web;
    mqttConfig_t mqtt;
    tuyaConfig_t tuyaKeys;
    uint8_t tuyaBinded = 0;

    char version[10] = "";
    uint16_t refreshRate = 60;
    uint8_t sleep = 1;
    uint16_t checksum = 0;
};

/*==============================================================================
 Public Variables Declaration
==============================================================================*/
extern const char *MODES[];
extern const char *GIT_TAG;
extern const char *GIT_REV;
extern const char *GIT_BRANCH;
extern const char *BUILD_TIME;

/*==============================================================================
 Public Functions Declaration
==============================================================================*/

class Config
{
public:
    Config();
    int8_t erase();
    int8_t begin();
    int8_t read();
    int8_t write();
    uint8_t verify();
    int16_t calculateChecksum();
    config_t values;

private:
    nvs_handle_t nvsHandle;
};

extern Config config;

#endif /* CONFIG_H */