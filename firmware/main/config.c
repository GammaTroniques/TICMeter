/**
 * @file config.cpp
 * @author Dorian Benech
 * @brief
 * @version 1.0
 * @date 2023-10-11
 *
 * @copyright Copyright (c) 2023 GammaTroniques
 *
 */

/*==============================================================================
 Local Include
===============================================================================*/
#include "config.h"
#include "linky.h"
/*==============================================================================
 Local Define
===============================================================================*/
#define TAG "Config"

/*==============================================================================
 Local Macro
===============================================================================*/

/*==============================================================================
 Local Type
===============================================================================*/
struct config_item_t
{
    const char *name;
    const LinkyLabelType type;
    void *value;
    size_t size;
    nvs_open_mode_t access;
    nvs_handle_t handle;
};
/*==============================================================================
 Local Function Declaration
===============================================================================*/

/*==============================================================================
Public Variable
===============================================================================*/
const char *MODES[] = {"WEB", "MQTT", "MQTT_HA", "ZIGBEE", "MATTER", "TUYA"};
config_t config_values = {};

/*==============================================================================
 Local Variable
===============================================================================*/
// clang-format off

static struct config_item_t config_items[] = {
    {"wifi-ssid",   STRING, &config_values.ssid,        sizeof(config_values.ssid),         NVS_READWRITE, 0},
    {"wifi-pw"  ,   STRING, &config_values.password,    sizeof(config_values.password),     NVS_READWRITE, 0},

    {"linky-mode",   UINT8, &config_values.linkyMode,   sizeof(config_values.linkyMode),    NVS_READWRITE, 0},
    {"connect-mode", UINT8, &config_values.mode,        sizeof(config_values.mode),         NVS_READWRITE, 0},

    {"web-conf",      BLOB, &config_values.web,         sizeof(config_values.web),          NVS_READWRITE, 0},
    {"mqtt-conf",     BLOB, &config_values.mqtt,        sizeof(config_values.mqtt),         NVS_READWRITE, 0},
    {"tuya-keys",     BLOB, &config_values.tuya,    sizeof(config_values.tuya),     NVS_READWRITE, 0},

    {"version",     STRING, &config_values.version,     sizeof(config_values.version),      NVS_READWRITE, 0},
    {"refresh",     UINT16, &config_values.refreshRate, sizeof(config_values.refreshRate),  NVS_READWRITE, 0},
    {"sleep",        UINT8, &config_values.sleep,       sizeof(config_values.sleep),        NVS_READWRITE, 0},
};
static const int32_t config_items_size = sizeof(config_items) / sizeof(config_items[0]);
// clang-format on

/*==============================================================================
Function Implementation
===============================================================================*/

int16_t config_calculate_checksum()
{
    // int16_t checksum = 0;
    // int i;
    // for (i = 0; i < sizeof(config_t) - sizeof(config_values.checksum); i++)
    // {
    //     checksum += ((uint8_t *)&config_values)[i];
    // }
    // ESP_LOGI(TAG, "Config calculated checksum: %x, i: %d", checksum, i);
    // return checksum;
    return 0;
}

int8_t config_erase()
{
    config_t blank_config = {
        .refreshRate = 60,
        .sleep = 1,
        .linkyMode = AUTO,
        .mode = MODE_MQTT_HA,
    };
    config_values = blank_config;
    return 0;
}

int8_t config_begin()
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

    for (int i = 0; i < config_items_size; i++)
    {
        err = nvs_open(config_items[i].name, config_items[i].access, &config_items[i].handle);
        if (err != ESP_OK)
        {
            ESP_LOGE(TAG, "Error (%s) opening %s\n", esp_err_to_name(err), config_items[i].name);
            continue;
        }
    }

    // err = nvs_open("config", NVS_READWRITE, &nvsHandle);
    // if (err != ESP_OK)
    // {
    //     printf("Error (%s) opening NVS handle!\n", esp_err_to_name(err));
    // }
    // else
    // {
    //     printf("Done\n");
    // }

    config_read();
    // ESP_LOG_BUFFER_HEXDUMP(NVS_TAG, &this->values, sizeof(config_t), ESP_LOG_INFO);
    // ESP_LOGI(NVS_TAG, "Config read checksum: %x", this->values.checksum);
    // ESP_LOGI(NVS_TAG, "Config calculated checksum: %x", this->confif_calculate_checksum());
    // if (this->values.checksum != this->confif_calculate_checksum())
    // {
    //     ESP_LOGI(NVS_TAG, "Config checksum error: %x != %x", this->values.checksum, this->confif_calculate_checksum());
    //     this->config_erase();
    //     ESP_LOGI(NVS_TAG, "Config erased");
    //     this->write();
    //     ESP_LOG_BUFFER_HEXDUMP(NVS_TAG, &this->values, sizeof(config_t), ESP_LOG_INFO);
    //     return 1;
    // }

    ESP_LOGI(TAG, "Config OK");
    return 0;
}

int8_t config_read()
{
    // size_t bytesRead = sizeof(config_t);
    // esp_err_t err = nvs_get_blob(nvsHandle, "config", &this->values, &bytesRead);
    // if (err != ESP_OK)
    // {
    //     ESP_LOGI(NVS_TAG, "Error (%s) reading!\n", esp_err_to_name(err));
    //     return 1;
    // }
    esp_err_t err = 0;
    size_t totalBytesRead = 0;
    for (int i = 0; i < config_items_size; i++)
    {
        size_t bytesRead = 0;
        switch (config_items[i].type)
        {
        case UINT8:
            err = nvs_get_u8(config_items[i].handle, config_items[i].name, (uint8_t *)config_items[i].value);
            bytesRead = sizeof(uint8_t);
            break;
        case UINT16:
            err = nvs_get_u16(config_items[i].handle, config_items[i].name, (uint16_t *)config_items[i].value);
            bytesRead = sizeof(uint16_t);
            break;
        case UINT32:
            err = nvs_get_u32(config_items[i].handle, config_items[i].name, (uint32_t *)config_items[i].value);
            bytesRead = sizeof(uint32_t);
            break;
        case UINT64:
            err = nvs_get_u64(config_items[i].handle, config_items[i].name, (uint64_t *)config_items[i].value);
            bytesRead = sizeof(uint64_t);
            break;
        case STRING:
            err = nvs_get_str(config_items[i].handle, config_items[i].name, (char *)config_items[i].value, &config_items[i].size);
            break;
        case BLOB:
            err = nvs_get_blob(config_items[i].handle, config_items[i].name, config_items[i].value, &config_items[i].size);
            break;
        default:
            break;
        }
        if (err != ESP_OK)
        {
            ESP_LOGE(TAG, "Error (%s) reading %s\n", esp_err_to_name(err), config_items[i].name);
            continue;
        }
        totalBytesRead += bytesRead;
    }

    ESP_LOGI(TAG, "Config read %d bytes", totalBytesRead);
    return 0;
}

int8_t config_write()
{
    // this->values.checksum = this->confif_calculate_checksum();
    // esp_err_t err = nvs_set_blob(nvsHandle, "config", &this->values, sizeof(config_t));
    // if (err != ESP_OK)
    // {
    //     ESP_LOGI(NVS_TAG, "Error (%s) writing!\n", esp_err_to_name(err));
    //     return 1;
    // }
    // err = nvs_commit(nvsHandle);
    // if (err != ESP_OK)
    // {
    //     ESP_LOGI(NVS_TAG, "Error (%s) committing!\n", esp_err_to_name(err));
    //     return 1;
    // }

    esp_err_t err = 0;
    size_t totalBytesWritten = 0;
    for (int i = 0; i < config_items_size; i++)
    {
        size_t bytesWritten = 0;
        switch (config_items[i].type)
        {
        case UINT8:
            err = nvs_set_u8(config_items[i].handle, config_items[i].name, *(uint8_t *)config_items[i].value);
            bytesWritten = sizeof(uint8_t);
            break;
        case UINT16:
            err = nvs_set_u16(config_items[i].handle, config_items[i].name, *(uint16_t *)config_items[i].value);
            bytesWritten = sizeof(uint16_t);
            break;
        case UINT32:
            err = nvs_set_u32(config_items[i].handle, config_items[i].name, *(uint32_t *)config_items[i].value);
            bytesWritten = sizeof(uint32_t);
            break;
        case UINT64:
            err = nvs_set_u64(config_items[i].handle, config_items[i].name, *(uint64_t *)config_items[i].value);
            bytesWritten = sizeof(uint64_t);
            break;
        case STRING:
            err = nvs_set_str(config_items[i].handle, config_items[i].name, (char *)config_items[i].value);
            bytesWritten = strlen((char *)config_items[i].value);
            break;
        case BLOB:
            err = nvs_set_blob(config_items[i].handle, config_items[i].name, config_items[i].value, config_items[i].size);
            bytesWritten = config_items[i].size;
            break;
        default:
            break;
        }
        if (err != ESP_OK)
        {
            ESP_LOGE(TAG, "Error (%s) writing %s\n", esp_err_to_name(err), config_items[i].name);
            continue;
        }

        err = nvs_commit(config_items[i].handle);
        if (err != ESP_OK)
        {
            ESP_LOGE(TAG, "Error (%s) committing %s\n", esp_err_to_name(err), config_items[i].name);
            continue;
        }
        totalBytesWritten += bytesWritten;
    }
    ESP_LOGI(TAG, "Config written %d bytes", totalBytesWritten);

    return 0;
}

uint8_t config_verify()
{
    switch (config_values.mode)
    {
    case MODE_WEB:
    case MODE_MQTT:
    case MODE_MQTT_HA:
    case MODE_TUYA:
        if (strlen(config_values.ssid) == 0 || strlen(config_values.password) == 0)
        {
            // No SSID or password
            return 1;
        }
        break;
    default:
        break;
    }

    switch (config_values.mode)
    {
    case MODE_WEB:
        if (strlen(config_values.web.host) == 0 || strlen(config_values.web.token) == 0 || strlen(config_values.web.postUrl) == 0 || strlen(config_values.web.configUrl) == 0)
        {
            // No web host, token, postUrl or configUrl
            return 1;
        }
        break;
    case MODE_MQTT:
    case MODE_MQTT_HA:
        if (strlen(config_values.mqtt.host) == 0 || strlen(config_values.mqtt.username) == 0 || strlen(config_values.mqtt.password) == 0 || strlen(config_values.mqtt.topic) == 0)
        {
            // No MQTT host, username, password or topic
            return 1;
        }
        break;

    case MODE_TUYA:
        if (strlen(config_values.tuya.product_id) == 0 || strlen(config_values.tuya.device_uuid) == 0 || strlen(config_values.tuya.device_auth) == 0 || config_values.tuya.pairing_state != TUYA_PAIRED)
        {
            // No Tuya key, id, version or region
            return 1;
        }
        // if (config_values.tuya.pairing_state == TUYA_NOT_CONFIGURED)
        // {
        //     // Tuya not binded
        //     return 0;
        // }
        break;
    default:
        break;
    }
    return 0;
}

uint8_t factory_reset()
{

    // clear nvs
    esp_err_t err = nvs_flash_erase();
    if (err != ESP_OK)
    {
        ESP_LOGE(TAG, "Error (%s) erasing NVS!\n", esp_err_to_name(err));
        return 1;
    }
    ESP_LOGI(TAG, "NVS erased");

    config_erase();
    config_write();
    ESP_LOGI(TAG, "Config erased");
    return 0;
}
