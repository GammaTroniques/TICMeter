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
#include "zigbee.h"
#include "esp_efuse.h"
#include "efuse_table.h"
#include "esp_efuse_table.h"
#include "esp_vfs.h"
#include "esp_spiffs.h"
#include "esp_hmac.h"
#include "nvs_sec_provider.h"
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
    const linky_label_type_t type;
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
const char *const MODES[] = {
    [MODE_NONE] = "NONE",
    [MODE_WEB] = "WEB",
    [MODE_MQTT] = "MQTT",
    [MODE_MQTT_HA] = "MQTT_HA",
    [MODE_ZIGBEE] = "ZIGBEE",
    [MODE_MATTER] = "MATTER",
    [MODE_TUYA] = "TUYA",
};

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
    {"init",            UINT8,  &config_values.initialized,     sizeof(config_values.initialized),      &config_handle},
    {"wifi-ssid",       STRING, &config_values.ssid,            sizeof(config_values.ssid),             &config_handle},
    {"wifi-pw"  ,       STRING, &config_values.password,        sizeof(config_values.password),         &config_handle},
    
    {"linky-mode",      UINT8,  &config_values.linky_mode,       sizeof(config_values.linky_mode),      &config_handle},
    {"last-linky-mode", UINT8,  &config_values.last_linky_mode, sizeof(config_values.last_linky_mode),  &config_handle},

    {"connect-mode",    UINT8, &config_values.mode,             sizeof(config_values.mode),             &config_handle},
    
    {"web-conf",        BLOB,   &config_values.web,             sizeof(config_values.web),              &config_handle},
    {"mqtt-conf",       BLOB,   &config_values.mqtt,            sizeof(config_values.mqtt),             &config_handle},
    {"tuya-keys",       BLOB,   &config_values.tuya,            sizeof(config_values.tuya),             &ro_config_handle},
    {"pairing",         UINT8,  &config_values.pairing_state,   sizeof(config_values.pairing_state),    &config_handle},
    {"zigbee",          BLOB,   &config_values.zigbee,          sizeof(config_values.zigbee),           &config_handle},
 
    {"version",         STRING, &config_values.version,         sizeof(config_values.version),          &config_handle},
    {"refresh",         UINT16, &config_values.refresh_rate,     sizeof(config_values.refresh_rate),    &config_handle},
    {"sleep",           UINT8,  &config_values.sleep,           sizeof(config_values.sleep),            &config_handle},
    {"index-offset",    BLOB,   &config_values.index_offset,    sizeof(config_values.index_offset),     &config_handle},
    {"boot-pairing",    UINT8,  &config_values.boot_pairing,    sizeof(config_values.boot_pairing),     &config_handle},

};
static const int32_t config_items_size = sizeof(config_items) / sizeof(config_items[0]);

// clang-format on

nvs_sec_cfg_t nvs_ro_sec_cfg = {0};
nvs_sec_scheme_t *sec_scheme_handle = NULL;
nvs_sec_config_hmac_t sec_scheme_cfg = {
    .hmac_key_id = HMAC_KEY5,
};
/*==============================================================================
Function Implementation
===============================================================================*/

int8_t config_erase()
{
    config_t blank_config = {
        .initialized = 1,
        .refresh_rate = 60,
        .sleep = 1,
        .linky_mode = AUTO,
        .last_linky_mode = NONE,
        .mode = MODE_MQTT_HA,

        .mqtt.port = 1883,
        .pairing_state = TUYA_NOT_CONFIGURED,
        .zigbee.state = ZIGBEE_NOT_CONFIGURED,
        .index_offset = {0},
    };

    snprintf(blank_config.mqtt.topic, sizeof(blank_config.mqtt.topic), "TICMeter/%s", efuse_values.mac_address + 6);
    config_values = blank_config;
    return 0;
}

int8_t config_begin()
{
    uint8_t want_init = 0;
    config_efuse_init();
    config_init_spiffs();

    // -------------------------- NVS init --------------------------
    esp_err_t err = nvs_flash_init();
    if (err == ESP_ERR_NVS_NO_FREE_PAGES || err == ESP_ERR_NVS_NEW_VERSION_FOUND)
    {
        // NVS partition was truncated and needs to be erased
        // Retry nvs_flash_init
        ESP_LOGW(TAG, "NVS partition was truncated and needs to be erased");
        err = nvs_flash_erase();
        if (err != ESP_OK)
        {
            ESP_LOGE(TAG, "Error (%s) erasing NVS!", esp_err_to_name(err));
            return 1;
        }
        err = nvs_flash_init();
        if (err != ESP_OK)
        {
            ESP_LOGE(TAG, "Error (%s) initializing NVS!", esp_err_to_name(err));
            return 1;
        }
        want_init = 1;
    }
    if (err != ESP_OK)
    {
        ESP_LOGE(TAG, "Error (%s) initializing NVS!", esp_err_to_name(err));
        return 1;
    }

    err = nvs_open(CONFIG_NAMESPACE, NVS_READWRITE, &config_handle);
    if (err != ESP_OK)
    {
        ESP_LOGE(TAG, "Error (%s) opening NVS handle!", esp_err_to_name(err));
    }

    // ------------------ Read-only partition ------------------
    err = nvs_sec_provider_register_hmac(&sec_scheme_cfg, &sec_scheme_handle);
    if (err != ESP_OK)
    {
        ESP_LOGE(TAG, "Error (%s) registering HMAC!", esp_err_to_name(err));
        return 1;
    }

    err = nvs_flash_read_security_cfg_v2(sec_scheme_handle, &nvs_ro_sec_cfg);
    if (err != ESP_OK)
    {
        if (err == ESP_ERR_NVS_SEC_HMAC_KEY_NOT_FOUND)
        {
            err = nvs_flash_generate_keys_v2(sec_scheme_handle, &nvs_ro_sec_cfg);
            if (err != ESP_OK)
            {
                ESP_LOGE(TAG, "Failed to generate NVS encr-keys!");
                return 1;
            }
        }
        ESP_LOGE(TAG, "Failed to read NVS security cfg!");
        return 1;
    }

    uint8_t want_init_ro = 0;
    err = nvs_flash_secure_init_partition(RO_PARTITION, &nvs_ro_sec_cfg);
    if (err == ESP_ERR_NVS_NO_FREE_PAGES || err == ESP_ERR_NVS_NEW_VERSION_FOUND)
    {
        // NVS partition was truncated and needs to be erased
        // Retry nvs_flash_init
        err = nvs_flash_erase_partition(RO_PARTITION);
        if (err != ESP_OK)
        {
            ESP_LOGE(TAG, "Error (%s) erasing ro NVS!", esp_err_to_name(err));
            return 1;
        }
        err = nvs_flash_secure_init_partition(RO_PARTITION, &nvs_ro_sec_cfg);
        want_init_ro = 1;
        if (err != ESP_OK)
        {
            ESP_LOGE(TAG, "Error (%s) initializing ro NVS!", esp_err_to_name(err));
            return 1;
        }
    }
    if (err != ESP_OK)
    {
        ESP_LOGE(TAG, "Error (%s) initializing ro NVS!", esp_err_to_name(err));
        return 1;
    }
    err = nvs_open_from_partition(RO_PARTITION, RO_NAMESPACE, NVS_READONLY, &ro_config_handle);

    if (err == ESP_ERR_NVS_NOT_FOUND)
    {
        ESP_LOGW(TAG, "Ro config not found, creating default config tuya");
        want_init_ro = 1;
    }
    else if (err != ESP_OK)
    {
        ESP_LOGE(TAG, "Error (0x%x %s) opening ro NVS handle!", err, esp_err_to_name(err));
        return 1;
    }

    // -------------------------- Read config --------------------------

    config_read();
    if (want_init || config_values.initialized == 0)
    {
        ESP_LOGW(TAG, "Config not found, creating default config");
        config_erase();
    }
    if (want_init_ro)
    {
        config_rw();
        ESP_LOGW(TAG, "Config not found, creating default config tuya");
        memset(&config_values.tuya, 0, sizeof(config_values.tuya));
    }
    config_write();
    config_read();
    if (config_values.initialized)
    {
        ESP_LOGI(TAG, "Config OK");
    }
    else
    {
        ESP_LOGE(TAG, "Config not OK !");
    }

    uint8_t edited = 0;

    if (config_values.refresh_rate < 30)
    {
        config_values.refresh_rate = 30;
        edited = 1;
    }

    if (strnlen(config_values.mqtt.topic, sizeof(config_values.mqtt.topic)) == 0)
    {
        ESP_LOGW(TAG, "MQTT topic not set, using default");
        snprintf(config_values.mqtt.topic, sizeof(config_values.mqtt.topic), "TICMeter/%s", efuse_values.mac_address + 6);
        edited = 1;
    }

    if (config_values.mqtt.port == 0)
    {
        config_values.mqtt.port = 1883;
        edited = 1;
    }

    if (config_values.mode == MODE_NONE || config_values.mode >= MODE_LAST)
    {
        config_values.mode = MODE_MQTT_HA;
        edited = 1;
    }

    if (edited)
    {
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
            ESP_LOGE(TAG, "Error reading %s: value not found.", config_items[i].name);
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
            // ESP_LOGI(TAG, "Writing %s", config_items[i].name);
            // ESP_LOG_BUFFER_HEXDUMP(TAG, config_items[i].value, bytesWritten, ESP_LOG_INFO);
            break;
        default:
            break;
        }
        if (err == ESP_ERR_NVS_READ_ONLY)
        {
            ESP_LOGE(TAG, "Error writing %s: read-only partition (enable write with 'rw' command)", config_items[i].name);
            continue;
        }
        if (err == ESP_ERR_NVS_NOT_ENOUGH_SPACE)
        {
        }
        else if (err != ESP_OK)
        {
            ESP_LOGE(TAG, "Error (0x%x %s) writing %s", err, esp_err_to_name(err), config_items[i].name);
            continue;
        }

        err = nvs_commit(*config_items[i].handle);
        if (err != ESP_OK)
        {
            ESP_LOGE(TAG, "Error (%s) committing %s", esp_err_to_name(err), config_items[i].name);
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
        if (strlen(config_values.tuya.device_uuid) == 0 || strlen(config_values.tuya.device_auth) == 0 || config_values.pairing_state != TUYA_PAIRED)
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
    case MODE_ZIGBEE:
        if (config_values.zigbee.state == ZIGBEE_NOT_CONFIGURED)
        {
            // Zigbee not binded
            return 1;
        }
        break;
    default:
        break;
    }
    return 0;
}

uint8_t config_rw()
{
    esp_err_t err = 0;

    err = nvs_flash_deinit_partition(RO_PARTITION);
    // open read-write partition
    err = nvs_flash_secure_init_partition(RO_PARTITION, &nvs_ro_sec_cfg);
    if (err == ESP_ERR_NVS_NO_FREE_PAGES || err == ESP_ERR_NVS_NEW_VERSION_FOUND)
    {
        // NVS partition was truncated and needs to be erased
        // Retry nvs_flash_init
        ESP_ERROR_CHECK(nvs_flash_erase_partition(RO_PARTITION));
        err = nvs_flash_secure_init_partition(RO_PARTITION, &nvs_ro_sec_cfg);
    }
    if (err != ESP_OK)
    {
        ESP_LOGE(TAG, "Error (%s) initializing ro NVS!", esp_err_to_name(err));
        return 1;
    }
    err = nvs_open_from_partition(RO_PARTITION, RO_NAMESPACE, NVS_READWRITE, &ro_config_handle);
    if (err != ESP_OK)
    {
        ESP_LOGE(TAG, "Error (%s) opening ro NVS handle!", esp_err_to_name(err));
    }
    ESP_LOGI(TAG, "Successfully opened in RW");
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
        ESP_LOGI(TAG, "Coding Scheme NONE");
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
        ESP_LOGI(TAG, "Coding Scheme RS (Reed-Solomon coding)");
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
        ESP_LOGE(TAG, "Error 0x%x reading mac!", err);
    }
    else
    {
        snprintf(efuse_values.mac_address, sizeof(efuse_values.mac_address), "%02X%02X%02X%02X%02X%02X", mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
        ESP_LOGI(TAG, "MAC address: %s", efuse_values.mac_address);
    }

    err = esp_efuse_read_field_blob(ESP_EFUSE_USER_DATA_SERIALNUMBER, efuse_values.serial_number, (sizeof(efuse_values.serial_number) - 1) * 8);
    if (err != ESP_OK)
    {
        ESP_LOGE(TAG, "Error 0x%x reading serial number!", err);
    }

    if (strnlen(efuse_values.serial_number, sizeof(efuse_values.serial_number)) == 0)
    {
        ESP_LOGE(TAG, "Serial number is empty!");
    }
    ESP_LOGI(TAG, "Serial number: %s", efuse_values.serial_number);

    err = esp_efuse_read_field_blob(ESP_EFUSE_USER_DATA_HWVERSION, efuse_values.hw_version, 3 * 8);
    if (err != ESP_OK)
    {
        ESP_LOGE(TAG, "Error 0x%x reading hardware version!", err);
    }

    if (efuse_values.hw_version[0] == 0 && efuse_values.hw_version[1] == 0 && efuse_values.hw_version[2] == 0)
    {
        ESP_LOGE(TAG, "Hardware version is empty!");
    }
    else
    {
        ESP_LOGI(TAG, "Hardware version: %d.%d.%d", efuse_values.hw_version[0], efuse_values.hw_version[1], efuse_values.hw_version[2]);
    }

    return 0;
}

uint8_t config_efuse_write(const char *serialnumber, uint8_t len, const uint8_t *hw_version)
{
    esp_err_t err = ESP_OK;
    if (len > sizeof(efuse_values.serial_number) - 1)
    {
        ESP_LOGE(TAG, "Serial number too long!");
        return 1;
    }
    err = config_efuse_read();

    esp_efuse_coding_scheme_t coding_scheme = esp_efuse_get_coding_scheme(EFUSE_BLK3);
    if (coding_scheme == EFUSE_CODING_SCHEME_RS)
    {
        err = esp_efuse_batch_write_begin();
        if (err != ESP_OK)
        {
            ESP_LOGE(TAG, "Error 0x%x starting batch write!", err);
            return 1;
        }
    }

    printf("Writing SN \"%s\" to efuse\n", serialnumber);

    if (strlen(efuse_values.serial_number) > 0)
    {
        printf("Can't write serial number: already written: %s\n", efuse_values.serial_number);
    }
    else
    {
        len = (len > 12) ? 12 : len;
        ESP_LOGI(TAG, "Writing serial number length %d", len);
        err = esp_efuse_write_field_blob(ESP_EFUSE_USER_DATA_SERIALNUMBER, serialnumber, len * 8);
        if (err != ESP_OK)
        {
            printf("Error 0x%x writing serial number!\n", err);
        }
        else
        {
            printf("Serial number written\n");
        }
    }
    if (hw_version != NULL)
    {
        if (efuse_values.hw_version[0] != 0 || efuse_values.hw_version[1] != 0 || efuse_values.hw_version[2] != 0)
        {
            printf("Can't write hardware version: already written: %d.%d.%d\n", efuse_values.hw_version[0], efuse_values.hw_version[1], efuse_values.hw_version[2]);
        }
        else
        {
            err = esp_efuse_write_field_blob(ESP_EFUSE_USER_DATA_HWVERSION, hw_version, 3 * 8);
            if (err != ESP_OK)
            {
                printf("Error 0x%x writing hardware version!\n", err);
            }
            else
            {
                printf("Hardware version written\n");
            }
        }
    }

    if (coding_scheme == EFUSE_CODING_SCHEME_RS)
    {
        err = esp_efuse_batch_write_commit();
        if (err != ESP_OK)
        {
            ESP_LOGE(TAG, "Error 0x%x committing batch write!", err);
            return 1;
        }
    }

    err = config_efuse_read();
    if (err != ESP_OK)
    {
        ESP_LOGE(TAG, "Error 0x%x rereading serial number!", err);
        return 1;
    }

    return 0;
}

uint8_t config_factory_reset()
{

    esp_err_t err = nvs_flash_erase();
    if (err != ESP_OK)
    {
        ESP_LOGE(TAG, "Failed to erase NVS (%s 0x%x)", esp_err_to_name(err), err);
    }
    else
    {
        ESP_LOGI(TAG, "NVS erased");
    }

    err = config_erase_partition("nvs");
    if (err != ESP_OK)
    {
        ESP_LOGE(TAG, "Failed to erase partition nvs (%s 0x%x)", esp_err_to_name(err), err);
    }
    else
    {
        ESP_LOGI(TAG, "Partition nvs erased");
    }
    config_begin(); // ??
    config_erase();
    config_write();

    zigbee_factory_reset();

    vTaskDelay(1000 / portTICK_PERIOD_MS);
    hard_restart();
    return 0;
}

esp_err_t config_erase_partition(const char *partition_label)
{
    esp_err_t err = nvs_flash_erase_partition(partition_label);
    if (err != ESP_OK)
    {
        ESP_LOGE(TAG, "Failed to erase partition %s (%s 0x%x)", partition_label, esp_err_to_name(err), err);
    }
    else
    {
        ESP_LOGI(TAG, "Partition %s erased", partition_label);
    }
    return err;
}

uint32_t config_get_hw_version()
{
    return (efuse_values.hw_version[0] << 16) | (efuse_values.hw_version[1] << 8) | efuse_values.hw_version[2];
}