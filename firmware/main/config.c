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

#define CONFIG_NAMESPACE "config"
#define RO_PARTITION "ro_nvs"
#define RO_NAMESPACE "ro_config"
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
    nvs_handle_t *handle;
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

static nvs_handle_t config_handle = 0;
static nvs_handle_t ro_config_handle = 0;
static struct config_item_t config_items[] = {
    {"wifi-ssid",   STRING, &config_values.ssid,            sizeof(config_values.ssid),             &config_handle},
    {"wifi-pw"  ,   STRING, &config_values.password,        sizeof(config_values.password),         &config_handle},
    
    {"linky-mode",   UINT8, &config_values.linkyMode,       sizeof(config_values.linkyMode),        &config_handle},
    {"connect-mode", UINT8, &config_values.mode,            sizeof(config_values.mode),             &config_handle},
    
    {"web-conf",      BLOB, &config_values.web,             sizeof(config_values.web),              &config_handle},
    {"mqtt-conf",     BLOB, &config_values.mqtt,            sizeof(config_values.mqtt),             &config_handle},
    {"tuya-keys",     BLOB, &config_values.tuya,            sizeof(config_values.tuya),             &ro_config_handle},
    {"pairing",      UINT8, &config_values.pairing_state,   sizeof(config_values.pairing_state),    &config_handle},
 
    {"version",     STRING, &config_values.version,         sizeof(config_values.version),          &config_handle},
    {"refresh",     UINT16, &config_values.refreshRate,     sizeof(config_values.refreshRate),      &config_handle},
    {"sleep",        UINT8, &config_values.sleep,           sizeof(config_values.sleep),            &config_handle},
};
static const int32_t config_items_size = sizeof(config_items) / sizeof(config_items[0]);

// clang-format on

/*==============================================================================
Function Implementation
===============================================================================*/

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
    uint8_t want_init = 0;
    esp_err_t err = nvs_flash_init();
    if (err == ESP_ERR_NVS_NO_FREE_PAGES || err == ESP_ERR_NVS_NEW_VERSION_FOUND)
    {
        // NVS partition was truncated and needs to be erased
        // Retry nvs_flash_init
        ESP_ERROR_CHECK(nvs_flash_erase());
        err = nvs_flash_init();
        want_init = 1;
    }
    if (err != ESP_OK)
    {
        ESP_LOGE(TAG, "Error (%s) initializing NVS!\n", esp_err_to_name(err));
        return 1;
    }

    err = nvs_open(CONFIG_NAMESPACE, NVS_READWRITE, &config_handle);
    if (err != ESP_OK)
    {
        ESP_LOGE(TAG, "Error (%s) opening NVS handle!\n", esp_err_to_name(err));
    }

    if (want_init)
    {
        ESP_LOGI(TAG, "Config not found, creating default config");
        config_erase();
        config_write();
    }

    // ------------------ Read-only partition ------------------
    want_init = 0;
    err = nvs_flash_init_partition(RO_PARTITION);
    if (err == ESP_ERR_NVS_NO_FREE_PAGES || err == ESP_ERR_NVS_NEW_VERSION_FOUND)
    {
        // NVS partition was truncated and needs to be erased
        // Retry nvs_flash_init
        ESP_ERROR_CHECK(nvs_flash_erase_partition(RO_PARTITION));
        err = nvs_flash_init_partition(RO_PARTITION);
        want_init = 1;
        if (err != ESP_OK)
        {
            ESP_LOGE(TAG, "Error (%s) initializing ro NVS!\n", esp_err_to_name(err));
            return 1;
        }
    }
    if (err != ESP_OK)
    {
        ESP_LOGE(TAG, "Error (%s) initializing ro NVS!\n", esp_err_to_name(err));
        return 1;
    }

    if (want_init)
    {
        err = nvs_open_from_partition(RO_PARTITION, RO_NAMESPACE, NVS_READWRITE, &ro_config_handle);
    }
    else
    {
        err = nvs_open_from_partition(RO_PARTITION, RO_NAMESPACE, NVS_READONLY, &ro_config_handle);
    }
    if (err != ESP_OK)
    {
        ESP_LOGE(TAG, "Error (0x%x %s) opening ro NVS handle!\n", err, esp_err_to_name(err));
    }

    if (want_init)
    {
        ESP_LOGI(TAG, "Config not found, creating default config tuya");
        config_erase();
        config_write();
    }

    config_read();
    ESP_LOGI(TAG, "Config OK");
    return 0;
}

int8_t config_read()
{
    esp_err_t err = 0;
    size_t totalBytesRead = 0;
    for (int i = 0; i < config_items_size; i++)
    {
        size_t bytesRead = 0;
        switch (config_items[i].type)
        {
        case UINT8:
            err = nvs_get_u8(*config_items[i].handle, config_items[i].name, (uint8_t *)config_items[i].value);
            bytesRead = sizeof(uint8_t);
            break;
        case UINT16:
            err = nvs_get_u16(*config_items[i].handle, config_items[i].name, (uint16_t *)config_items[i].value);
            bytesRead = sizeof(uint16_t);
            break;
        case UINT32:
            err = nvs_get_u32(*config_items[i].handle, config_items[i].name, (uint32_t *)config_items[i].value);
            bytesRead = sizeof(uint32_t);
            break;
        case UINT64:
            err = nvs_get_u64(*config_items[i].handle, config_items[i].name, (uint64_t *)config_items[i].value);
            bytesRead = sizeof(uint64_t);
            break;
        case STRING:
            err = nvs_get_str(*config_items[i].handle, config_items[i].name, (char *)config_items[i].value, &config_items[i].size);
            break;
        case BLOB:
            err = nvs_get_blob(*config_items[i].handle, config_items[i].name, config_items[i].value, &config_items[i].size);
            break;
        default:
            break;
        }
        if (err != ESP_OK)
        {
            ESP_LOGE(TAG, "Error (0x%x %s) reading %s", err, esp_err_to_name(err), config_items[i].name);
            continue;
        }
        totalBytesRead += bytesRead;
    }

    ESP_LOGI(TAG, "Config read %d bytes", totalBytesRead);
    return 0;
}

int8_t config_write()
{
    esp_err_t err = 0;
    size_t totalBytesWritten = 0;
    for (int i = 0; i < config_items_size; i++)
    {
        size_t bytesWritten = 0;
        switch (config_items[i].type)
        {
        case UINT8:
            err = nvs_set_u8(*config_items[i].handle, config_items[i].name, *(uint8_t *)config_items[i].value);
            bytesWritten = sizeof(uint8_t);
            break;
        case UINT16:
            err = nvs_set_u16(*config_items[i].handle, config_items[i].name, *(uint16_t *)config_items[i].value);
            bytesWritten = sizeof(uint16_t);
            break;
        case UINT32:
            err = nvs_set_u32(*config_items[i].handle, config_items[i].name, *(uint32_t *)config_items[i].value);
            bytesWritten = sizeof(uint32_t);
            break;
        case UINT64:
            err = nvs_set_u64(*config_items[i].handle, config_items[i].name, *(uint64_t *)config_items[i].value);
            bytesWritten = sizeof(uint64_t);
            break;
        case STRING:
            err = nvs_set_str(*config_items[i].handle, config_items[i].name, (char *)config_items[i].value);
            bytesWritten = strlen((char *)config_items[i].value);
            break;
        case BLOB:
            err = nvs_set_blob(*config_items[i].handle, config_items[i].name, config_items[i].value, config_items[i].size);
            ESP_LOGI(TAG, "Blob size: %d", config_items[i].size);
            bytesWritten = config_items[i].size;
            break;
        default:
            break;
        }
        if (err == ESP_ERR_NVS_READ_ONLY)
        {
            ESP_LOGE(TAG, "Error writing %s: read-only partition (enable write with 'rw' command)\n", config_items[i].name);
            continue;
        }
        else if (err != ESP_OK)
        {
            ESP_LOGE(TAG, "Error (0x%x %s) writing %s\n", err, esp_err_to_name(err), config_items[i].name);
            continue;
        }

        err = nvs_commit(*config_items[i].handle);
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
        if (strlen(config_values.tuya.product_id) == 0 || strlen(config_values.tuya.device_uuid) == 0 || strlen(config_values.tuya.device_auth) == 0 || config_values.pairing_state != TUYA_PAIRED)
        {
            // No Tuya key, id, version or region
            return 1;
        }
        // if (config_values.pairing_state == TUYA_NOT_CONFIGURED)
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

uint8_t config_rw()
{
    esp_err_t err = 0;

    err = nvs_flash_deinit_partition(RO_PARTITION);
    // open read-write partition
    err = nvs_flash_init_partition(RO_PARTITION);
    if (err == ESP_ERR_NVS_NO_FREE_PAGES || err == ESP_ERR_NVS_NEW_VERSION_FOUND)
    {
        // NVS partition was truncated and needs to be erased
        // Retry nvs_flash_init
        ESP_ERROR_CHECK(nvs_flash_erase_partition(RO_PARTITION));
        err = nvs_flash_init_partition(RO_PARTITION);
    }
    if (err != ESP_OK)
    {
        ESP_LOGE(TAG, "Error (%s) initializing ro NVS!\n", esp_err_to_name(err));
        return 1;
    }
    err = nvs_open_from_partition(RO_PARTITION, RO_NAMESPACE, NVS_READWRITE, &ro_config_handle);
    if (err != ESP_OK)
    {
        ESP_LOGE(TAG, "Error (%s) opening ro NVS handle!\n", esp_err_to_name(err));
    }
    printf("Successfully opened in RW\n");
    return 0;
}