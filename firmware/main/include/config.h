
#ifndef CONFIG_H
#define CONFIG_H

#include <stdio.h>
#include <inttypes.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_system.h"
#include "nvs_flash.h"
#include "nvs.h"
#include "esp_log.h"
#include "linky.h"

#define EEPROM_SIZE sizeof(config_t)

#define AP_SSID "Linky 5G Pfizer#9432"
#define AP_PASS ""
#define HOSTNAME "Linky"

#define NVS_TAG "NVS"

enum connectivity_t : uint8_t
{
    MODE_WEB,
    MODE_MQTT,
    MODE_MQTT_HA,
    MODE_ZIGBEE,
    MODE_MATTER,
    MODE_TUYA,
};

extern const char *MODES[];
extern const char *TUYA_SERVERS[];

#define CONNECTION_TYPE_WIFI 0
#define CONNECTION_TYPE_ZIGBEE 1
#define CONNECTION_TYPE_MATTER 2

#define MILLIS xTaskGetTickCount() * portTICK_PERIOD_MS

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
    uint8_t binded = 0;
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
    tuyaConfig_t tuya;

    char version[10] = "";
    uint16_t refreshRate = 60;
    uint8_t sleep = 1;
    uint16_t checksum = 0;
};

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

#endif