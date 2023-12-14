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
#include "esp_efuse.h"
#include "efuse_table.h"
#include "esp_efuse_table.h"
#include "esp_vfs.h"
#include "esp_spiffs.h"
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
static uint8_t config_efuse_init();
static esp_efuse_coding_scheme_t config_efuse_get_coding_scheme(void);
static int config_init_spiffs(void);
static uint8_t config_tuya_rw = 0;
/*==============================================================================
Public Variable
===============================================================================*/
const char *MODES[] = {"WEB", "MQTT", "MQTT_HA", "ZIGBEE", "MATTER", "TUYA"};
config_t config_values = {0};
efuse_t efuse_values = {0};
static esp_efuse_coding_scheme_t config_efuse_coding_scheme = EFUSE_CODING_SCHEME_NONE;
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
    snprintf(blank_config.mqtt.topic, sizeof(blank_config.mqtt.topic), "TICMeter/%s", efuse_values.macAddress + 6);
    config_values = blank_config;
    return 0;
}

int8_t config_begin()
{
    uint8_t want_init = 0;
    config_efuse_init();
    config_init_spiffs();
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
    if (err == ESP_ERR_NVS_NOT_FOUND)
    {
        ESP_LOGE(TAG, "Ro config not found, creating default config tuya");
        config_tuya_rw = 1;
        config_rw();
        config_write();
        config_tuya_rw = 0;
    }
    else if (err != ESP_OK)
    {
        ESP_LOGE(TAG, "Error (0x%x %s) opening ro NVS handle!", err, esp_err_to_name(err));
        return 1;
    }

    if (want_init)
    {
        ESP_LOGI(TAG, "Config not found, creating default config tuya");
        config_erase();
        config_write();
    }

    config_read();
    ESP_LOGI(TAG, "Config OK");

    if (config_values.refreshRate <= 30)
    {
        config_values.refreshRate = 60;
        config_write();
    }
    return 0;
}

static int config_init_spiffs(void)
{
    esp_vfs_spiffs_conf_t conf = {
        .base_path = "/spiffs",
        .partition_label = NULL,
        .max_files = 5,
        .format_if_mount_failed = true};

    esp_err_t err = esp_vfs_spiffs_register(&conf);
    if (err != ESP_OK)
    {
        if (err == ESP_FAIL)
        {
            ESP_LOGE(TAG, "Failed to mount or format filesystem");
        }
        else if (err == ESP_ERR_NOT_FOUND)
        {
            ESP_LOGE(TAG, "Failed to find SPIFFS partition");
        }
        else
        {
            ESP_LOGE(TAG, "Failed to initialize SPIFFS (%s)", esp_err_to_name(err));
        }
        return 1;
    }
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
        if (err == ESP_ERR_NVS_NOT_FOUND)
        {
            ESP_LOGE(TAG, "Error reading %s: value not found.\n", config_items[i].name);
            continue;
        }
        else if (err != ESP_OK)
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
        if (config_items[i].handle == &ro_config_handle && config_tuya_rw == 0)
        {
            continue;
        }
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
        if (err == ESP_ERR_NVS_NOT_ENOUGH_SPACE)
        {
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
    config_tuya_rw = 1;
    return 0;
}

static esp_efuse_coding_scheme_t config_efuse_get_coding_scheme(void)
{
    // The coding scheme is used for EFUSE_BLK1, EFUSE_BLK2 and EFUSE_BLK3.
    // We use EFUSE_BLK3 (custom block) to verify it.
    esp_efuse_coding_scheme_t coding_scheme = esp_efuse_get_coding_scheme(EFUSE_BLK3);
    if (coding_scheme == EFUSE_CODING_SCHEME_NONE)
    {
        ESP_LOGD(TAG, "Coding Scheme NONE");
#if CONFIG_IDF_TARGET_ESP32
    }
    else if (coding_scheme == EFUSE_CODING_SCHEME_3_4)
    {
        ESP_LOGI(TAG, "Coding Scheme 3/4");
    }
    else
    {
        ESP_LOGI(TAG, "Coding Scheme REPEAT");
    }
#else
    }
    else if (coding_scheme == EFUSE_CODING_SCHEME_RS)
    {
        ESP_LOGD(TAG, "Coding Scheme RS (Reed-Solomon coding)");
    }
#endif
    return coding_scheme;
}

static uint8_t config_efuse_init()
{
    config_efuse_coding_scheme = config_efuse_get_coding_scheme();
    config_efuse_read();
    return 0;
}

uint8_t config_efuse_read()
{
    esp_err_t err;

    // read mac
    uint8_t mac[6];
    err = esp_efuse_read_field_blob(ESP_EFUSE_MAC_FACTORY, mac, sizeof(mac) * 8);
    if (err != ESP_OK)
    {
        ESP_LOGE(TAG, "Error 0x%x reading mac!\n", err);
    }
    else
    {
        snprintf(efuse_values.macAddress, sizeof(efuse_values.macAddress), "%02X%02X%02X%02X%02X%02X", mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
        ESP_LOGI(TAG, "MAC address: %s", efuse_values.macAddress);
    }

    err = esp_efuse_read_field_blob(ESP_EFUSE_USER_DATA_SERIALNUMBER, efuse_values.serialNumber, (sizeof(efuse_values.serialNumber) - 1) * 8);
    if (err != ESP_OK)
    {
        ESP_LOGE(TAG, "Error 0x%x reading serial number!\n", err);
        return 1;
    }

    if (strnlen(efuse_values.serialNumber, sizeof(efuse_values.serialNumber)) == 0)
    {
        ESP_LOGE(TAG, "Serial number is empty!");
        return 2;
    }
    ESP_LOGI(TAG, "Serial number: %s", efuse_values.serialNumber);
    return 0;
}

uint8_t config_efuse_write(const char *serialnumber, uint8_t len)
{
    esp_err_t err = ESP_OK;
    if (len > sizeof(efuse_values.serialNumber) - 1)
    {
        ESP_LOGE(TAG, "Serial number too long!\n");
        return 1;
    }
    err = config_efuse_read();
    switch (err)
    {
    case 0:
        ESP_LOGE(TAG, "Serial number already set: can't write");
        return 1;
        break;
    case 1:
        ESP_LOGE(TAG, "Can't read serial number: can't write");
        return 1;
        break;
    default:
        break;
    }
    ESP_LOGI(TAG, "Writing SN \"%s\" to efuse", serialnumber);
    err = esp_efuse_write_field_blob(ESP_EFUSE_USER_DATA_SERIALNUMBER, serialnumber, len * 8);
    if (err != ESP_OK)
    {
        ESP_LOGE(TAG, "Error 0x%x writing serial number!\n", err);
        return 1;
    }
    ESP_LOGI(TAG, "Serial number written");

    err = config_efuse_read();
    if (err != ESP_OK)
    {
        ESP_LOGE(TAG, "Error 0x%x rereading serial number!\n", err);
        return 1;
    }

    return 0;
}