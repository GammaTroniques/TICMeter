
#ifndef CONFIG_H
#define CONFIG_H

#include <EEPROM.h> //store data in flash memory (the ESP32 does not have EEPROM)
#define EEPROM_SIZE sizeof(config_t)

#define AP_SSID "Linky 5G Pfizer#9432"

#define MODE_WEB 0
#define MODE_MQTT 1
#define MODE_MQTT_HA 2

#define CONNECTION_TYPE_WIFI 0
#define CONNECTION_TYPE_ZIGBEE 1
#define CONNECTION_TYPE_MATTER 2

#define V_CONDO_PIN 32            // io32
#define PAIRING_PIN (gpio_num_t)6 // io6
#define PAIRING_LED_PIN 5         // io2

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

struct config_t
{
    uint16_t magic = 0x1234; // magic number to check if a config is stored in EEPROM
    char ssid[50] = "";
    char password[50] = "";

    uint8_t connectionType = CONNECTION_TYPE_WIFI;
    uint8_t mode = MODE_WEB;
    webConfig_t web;
    mqttConfig_t mqtt;

    char version[10] = "";
    uint16_t refreshRate = 60;
    uint8_t enableDeepSleep = 1;
    uint8_t dataCount = 3;
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
    int16_t calculateChecksum();
    config_t values;

private:
};

#endif