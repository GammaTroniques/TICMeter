#include "config.h"

const char *MODES[] = {"WEB", "MQTT", "MQTT_HA", "ZIGBEE", "MATTER", "TUYA"};
const char *TUYA_SERVERS[] = {"m1.tuyacn.com", "m1.tuyaeu.com", "m1.tuyaus.com", "m1-ueaz.tuyaus.com", "m1-weaz.tuyaeu.com", "m1.tuyain.com"};
Config::Config()
{
}

int16_t Config::calculateChecksum()
{
    int16_t checksum = 0;
    for (int i = 0; i < sizeof(config_t) - sizeof(this->values.checksum); i++)
    {
        checksum += ((uint8_t *)&this->values)[i];
    }
    return checksum;
}

int8_t Config::erase()
{
    Config blank_config;
    this->values = blank_config.values;
    return 0;
}

int8_t Config::begin()
{
    esp_err_t err = nvs_flash_init();
    if (err == ESP_ERR_NVS_NO_FREE_PAGES || err == ESP_ERR_NVS_NEW_VERSION_FOUND)
    {
        // NVS partition was truncated and needs to be erased
        // Retry nvs_flash_init
        ESP_ERROR_CHECK(nvs_flash_erase());
        err = nvs_flash_init();
    }
    ESP_ERROR_CHECK(err);

    printf("\n");
    printf("Opening Non-Volatile Storage (NVS) handle... ");
    err = nvs_open("config", NVS_READWRITE, &nvsHandle);
    if (err != ESP_OK)
    {
        printf("Error (%s) opening NVS handle!\n", esp_err_to_name(err));
    }
    else
    {
        printf("Done\n");
    }

    this->read();
    ESP_LOGI(NVS_TAG, "Config read checksum: %x", this->values.checksum);
    ESP_LOGI(NVS_TAG, "Config calculated checksum: %x", this->calculateChecksum());
    if (this->values.checksum != this->calculateChecksum())
    {
        ESP_LOGI(NVS_TAG, "Config checksum error: %x != %x", this->values.checksum, this->calculateChecksum());
        this->erase();
        ESP_LOGI(NVS_TAG, "Config erased");
        this->write();
        return 1;
    }
    ESP_LOGI(NVS_TAG, "Config OK");
    return 0;
}

int8_t Config::read()
{
    size_t bytesRead = sizeof(config_t);
    esp_err_t err = nvs_get_blob(nvsHandle, "config", &this->values, &bytesRead);
    if (err != ESP_OK)
    {
        ESP_LOGI(NVS_TAG, "Error (%s) reading!\n", esp_err_to_name(err));
        return 1;
    }
    ESP_LOGI(NVS_TAG, "Config read %d bytes", bytesRead);
    return 0;
}

int8_t Config::write()
{
    this->values.checksum = this->calculateChecksum();
    esp_err_t err = nvs_set_blob(nvsHandle, "config", &this->values, sizeof(config_t));
    if (err != ESP_OK)
    {
        ESP_LOGI(NVS_TAG, "Error (%s) writing!\n", esp_err_to_name(err));
        return 1;
    }
    err = nvs_commit(nvsHandle);
    if (err != ESP_OK)
    {
        ESP_LOGI(NVS_TAG, "Error (%s) committing!\n", esp_err_to_name(err));
        return 1;
    }
    ESP_LOGI(NVS_TAG, "Config written");
    return 0;
}

uint8_t Config::verify()
{
    switch (config.values.mode)
    {
    case MODE_WEB:
    case MODE_MQTT:
    case MODE_MQTT_HA:
    case MODE_TUYA:
        if (strlen(config.values.ssid) == 0 || strlen(config.values.password) == 0)
        {
            // No SSID or password
            return 1;
        }
        break;
    default:
        break;
    }

    switch (config.values.mode)
    {
    case MODE_WEB:
        if (strlen(config.values.web.host) == 0 || strlen(config.values.web.token) == 0 || strlen(config.values.web.postUrl) == 0 || strlen(config.values.web.configUrl) == 0)
        {
            // No web host, token, postUrl or configUrl
            return 1;
        }
        break;
    case MODE_MQTT:
    case MODE_MQTT_HA:
        if (strlen(config.values.mqtt.host) == 0 || strlen(config.values.mqtt.username) == 0 || strlen(config.values.mqtt.password) == 0 || strlen(config.values.mqtt.topic) == 0)
        {
            // No MQTT host, username, password or topic
            return 1;
        }
        break;

    case MODE_TUYA:
        if (strlen(config.values.tuya.deviceId) == 0 || strlen(config.values.tuya.deviceSecret) == 0 || strlen(config.values.tuya.productId) == 0)
        {
            // No Tuya key, id, version or region
            return 1;
        }
        break;
    default:
        break;
    }
    return 0;
}