/**
 * @file tuya.cpp
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
#include "tuya.h"
#include "config.h"

#include "tuya_log.h"
#include "tuya_iot.h"
#include "tuya_ota.h"
#include "cJSON.h"
#include "qrcode.h"
#include "gpio.h"
#include "wifi.h"
#include "main.h"
#include "common.h"
#include "led.h"
#include "ota_zlib.h"

/* BLE */
#include "esp_ota_ops.h"
#include "nimble/nimble_port.h"
#include "nimble/nimble_port_freertos.h"
#include "host/ble_hs.h"
#include "host/util/util.h"
#include "console/console.h"
#include "services/gap/ble_svc_gap.h"
#include "nimble/ble.h"
#include "modlog/modlog.h"
#include "esp_peripheral.h"

#include "tuya_ble_service.h"
#include "tuya_log.h"
#include "MultiTimer.h"
/*==============================================================================
 Local Define
===============================================================================*/

#define TAG "TUYA"
#define GATT_SVR_SVC_ALERT_UUID 0x1811
#define GATT_SVR_CHR_SUP_NEW_ALERT_CAT_UUID 0x2A47
#define GATT_SVR_CHR_NEW_ALERT 0x2A46
#define GATT_SVR_CHR_SUP_UNR_ALERT_CAT_UUID 0x2A48
#define GATT_SVR_CHR_UNR_ALERT_STAT_UUID 0x2A45
#define GATT_SVR_CHR_ALERT_NOT_CTRL_PT 0x2A44
/*==============================================================================
 Local Macro
===============================================================================*/
#define STATE_ID2STR(S) \
    ((S) == STATE_IDLE ? "STATE_IDLE" : ((S) == STATE_START ? "STATE_START" : ((S) == STATE_DATA_LOAD ? "STATE_DATA_LOAD" : ((S) == STATE_TOKEN_PENDING ? "STATE_TOKEN_PENDING" : ((S) == STATE_ACTIVATING ? "STATE_ACTIVATING" : ((S) == STATE_STARTUP_UPDATE ? "STATE_STARTUP_UPDATE" : ((S) == STATE_MQTT_CONNECT_START ? "STATE_MQTT_CONNECT_START" : ((S) == STATE_MQTT_CONNECTING ? "STATE_MQTT_CONNECTING" : ((S) == STATE_MQTT_RECONNECT ? "STATE_MQTT_RECONNECT" : ((S) == STATE_MQTT_YIELD ? "STATE_MQTT_YIELD" : ((S) == STATE_RESTART ? "STATE_RESTART" : ((S) == STATE_RESET ? "STATE_RESET" : ((S) == STATE_EXIT ? "STATE_EXIT" : "Unknown")))))))))))))
/*==============================================================================
 Local Type
===============================================================================*/
typedef enum
{
    STATE_IDLE,
    STATE_START,
    STATE_DATA_LOAD,
    STATE_TOKEN_PENDING,
    STATE_ACTIVATING,
    STATE_STARTUP_UPDATE,
    STATE_MQTT_CONNECT_START,
    STATE_MQTT_CONNECTING,
    STATE_MQTT_RECONNECT,
    STATE_MQTT_YIELD,
    STATE_RESTART,
    STATE_RESET,
    STATE_STOP,
    STATE_EXIT,
} tuya_run_state_t;

typedef struct
{
    const char *look;
    const char *replace;
} str_replace_t;

/*==============================================================================
 Local Function Declaration
===============================================================================*/

static void tuya_qrcode_print(const char *productkey, const char *uuid);
static void tuya_user_event_handler_on(tuya_iot_client_t *client, tuya_event_msg_t *event);
static void tuya_iot_dp_download(tuya_iot_client_t *client, const char *json_dps);
static void tuya_link_app_task(void *pvParameters);
static void tuya_send_callback(int result, void *user_data);
void ble_token_get_cb(wifi_info_t wifi_info);
static char *tuya_verify_enum(char *input, const char *const *valid_values);

str_replace_t str_current_tarif_replace[] = {
    {"TH..", "BASE"},
    {"HC..", "Heures Creuses"},
    {"HP..", "Heures Pleines"},
    {"HCJB", "Heures Creuses Jours Bleus"},
    {"HCJW", "Heures Creuses Jours Blancs"},
    {"HCJR", "Heures Creuses Jours Rouges"},
    {"HPJB", "Heures Pleines Jours Bleus"},
    {"HPJW", "Heures Pleines Jours Blancs"},
    {"HPJR", "Heures Pleines Jours Rouges"},
    {"HN..", "Heures Normales"},
    {"PM..", "Heures de Pointe Mobile"},
    {NULL, NULL},
};

const char *const linky_tuya_valid_color[] = {
    "INCONNU",
    "BLEU",
    "BLANC",
    "ROUGE",
    NULL,
};

/*==============================================================================
Public Variable
===============================================================================*/
TaskHandle_t tuyaTaskHandle = NULL;
TaskHandle_t tuya_ble_pairing_task_handle = NULL;
bool tuya_state = false;

/*==============================================================================
 Local Variable
===============================================================================*/
static tuya_iot_client_t client = {0};
static tuya_ota_handle_t ota_handle = {
    .channel = 9,
};
static tuya_event_id_t lastEvent = TUYA_EVENT_RESET;
static uint8_t newEvent = 0;
/*==============================================================================
Function Implementation
===============================================================================*/

static void tuya_qrcode_print(const char *productkey, const char *uuid)
{
    ESP_LOGI(TAG, "https://smartapp.tuya.com/s/p?p=%s&uuid=%s&v=2.0", productkey, uuid);

    char urlbuf[255];
    snprintf(urlbuf, sizeof(urlbuf), "https://smartapp.tuya.com/s/p?p=%s&uuid=%s&v=2.0", productkey, uuid);
    qrcode_display(urlbuf);

    ESP_LOGI(TAG, "(Use this URL to generate a static QR code for the Tuya APP scan code binding)");
}

static void tuya_iot_dp_download(tuya_iot_client_t *client, const char *json_dps)
{
    ESP_LOGI(TAG, "Data point download value:%s", json_dps);

    /* Parsing json string to cJSON object */
    cJSON *dps = cJSON_Parse(json_dps);
    if (dps == NULL)
    {
        ESP_LOGI(TAG, "JSON parsing error, exit!");
        return;
    }

    cJSON *dp = cJSON_GetObjectItem(dps, "103");
    if (dp != NULL)
    {
        ESP_LOGI(TAG, "Received refresh rate: %d", dp->valueint);
        config_values.refresh_rate = dp->valueint;
        if (config_values.refresh_rate < 30)
        {
            config_values.refresh_rate = 30;
        }
        if (config_values.refresh_rate > 300)
        {
            config_values.refresh_rate = 300;
        }
        config_write();
    }

    /* relese cJSON DPS object */
    cJSON_Delete(dps);

    /* Report the received data to synchronize the switch status. */
    tuya_iot_dp_report_json(client, json_dps);
}

static void user_upgrade_notify_on(tuya_iot_client_t *client, cJSON *upgrade)
{
    TY_LOGI("----- Upgrade information -----");
    TY_LOGI("OTA Channel: %d", cJSON_GetObjectItem(upgrade, "type")->valueint);
    TY_LOGI("Version: %s", cJSON_GetObjectItem(upgrade, "version")->valuestring);
    TY_LOGI("Size: %s", cJSON_GetObjectItem(upgrade, "size")->valuestring);
    TY_LOGI("MD5: %s", cJSON_GetObjectItem(upgrade, "md5")->valuestring);
    TY_LOGI("HMAC: %s", cJSON_GetObjectItem(upgrade, "hmac")->valuestring);
    TY_LOGI("URL: %s", cJSON_GetObjectItem(upgrade, "url")->valuestring);
    TY_LOGI("HTTPS URL: %s", cJSON_GetObjectItem(upgrade, "httpsUrl")->valuestring);

    tuya_ota_begin(&ota_handle, upgrade);
}

static void tuya_user_event_handler_on(tuya_iot_client_t *client, tuya_event_msg_t *event)
{
    // ESP_LOGI(TAG, "TUYA_EVENT: %s", EVENT_ID2STR(event->id));
    switch (event->id)
    {
    case TUYA_EVENT_BIND_START:
        if (config_values.pairing_state != TUYA_BLE_PAIRING)
        {
            tuya_qrcode_print(client->config.productkey, client->config.uuid);
        }
        break;

    case TUYA_EVENT_MQTT_CONNECTED:
        ESP_LOGI(TAG, "Device MQTT Connected!");
        break;

    case TUYA_EVENT_DP_RECEIVE:
    {
        ESP_LOGI(TAG, "TUYA_EVENT_DP_RECEIVE");
        tuya_iot_dp_download(client, (const char *)event->value.asString);
        break;
    }
    case TUYA_EVENT_RESET:
        ESP_LOGI(TAG, "Tuya unbined");
        config_values.pairing_state = TUYA_NOT_CONFIGURED;
        config_values.boot_pairing = 1;
        esp_err_t err = config_erase_partition("nvs");
        if (err != ESP_OK)
        {
            ESP_LOGE(TAG, "nvs_flash_erase failed with 0x%X", err);
        }
        else
        {
            ESP_LOGI(TAG, "NVS erased, retry wifi");
            config_begin();
            config_write();
            esp_restart();
        }
        break;
    case TUYA_EVENT_BIND_TOKEN_ON:
        // start of binding

        break;
    case TUYA_EVENT_ACTIVATE_SUCCESSED:
        ESP_LOGI(TAG, "Tuya binded");
        config_values.pairing_state = TUYA_PAIRED;
        config_write();
        break;
    case TUYA_EVENT_DPCACHE_NOTIFY:
        TY_LOGI("Recv TUYA_EVENT_DPCACHE_NOTIFY");
        break;
    case TUYA_EVENT_UPGRADE_NOTIFY:
        user_upgrade_notify_on(client, event->value.asJSON);
        break;

    default:
        break;
    }
    newEvent = 1;
    lastEvent = event->id;
}

static void user_ota_event_cb(tuya_ota_handle_t *handle, tuya_ota_event_t *event)
{
    esp_err_t ret = ESP_OK;
    switch (event->id)
    {
    case TUYA_OTA_EVENT_START:
        ESP_LOGI(TAG, "OTA start");
        ret = ota_zlib_init();
        if (ret != ESP_OK)
        {
            ESP_LOGE(TAG, "OTA init error: %d", ret);
            tuya_ota_upgrade_status_report(handle, TUS_DOWNLOAD_ERROR_UNKONW);
        }
        led_start_pattern(LED_OTA_IN_PROGRESS);

        break;

    case TUYA_OTA_EVENT_ON_DATA:
        ESP_LOGI(TAG, "OTA data len: %d %d/%d", event->data_len, event->offset, event->file_size);
        ret = ota_zlib_write(event->data, event->data_len);
        if (ret != ESP_OK)
        {
            led_stop_pattern(LED_OTA_IN_PROGRESS);
            ESP_LOGE(TAG, "OTA write error: %d", ret);
            tuya_ota_upgrade_status_report(handle, TUS_DOWNLOAD_ERROR_UNKONW);
        }
        break;

    case TUYA_OTA_EVENT_FINISH:
        ESP_LOGI(TAG, "OTA finish");
        ret = ota_zlib_end();
        if (ret != ESP_OK)
        {
            ESP_LOGE(TAG, "OTA end error: %d", ret);
            tuya_ota_upgrade_status_report(handle, TUS_DOWNLOAD_ERROR_UNKONW);
        }
        led_stop_pattern(LED_OTA_IN_PROGRESS);
        break;
    case TUYA_OTA_EVENT_FAULT:
        ESP_LOGE(TAG, "OTA fault");
        led_stop_pattern(LED_OTA_IN_PROGRESS);

        break;
    }
}

static void tuya_link_app_task(void *pvParameters)
{
    int ret = OPRT_OK;
    const esp_app_desc_t *app_desc = esp_app_get_description();
    char version[10] = {0};
    // copy version before '-'
    strncpy(version, app_desc->version + 1, strcspn(app_desc->version + 1, "-"));
    ESP_LOGI(TAG, "Tuya version: %s", version);

    const tuya_iot_config_t tuya_config = {
        .productkey = TUYA_PRODUCT_ID,
        .uuid = config_values.tuya.device_uuid,
        .authkey = config_values.tuya.device_auth,
        .software_ver = version,
        .modules = NULL,
        .skill_param = NULL,
        .storage_namespace = "tuya",
        .firmware_key = NULL,
        .event_handler = tuya_user_event_handler_on,
    };

    /* Initialize Tuya device configuration */
    ret = tuya_iot_init(&client, &tuya_config);
    assert(ret == OPRT_OK);

    tuya_ota_init(&ota_handle, &(const tuya_ota_config_t){
                                   .client = &client,
                                   .event_cb = user_ota_event_cb,
                                   .range_size = 1024,
                                   .timeout_ms = 5000});

    /* Start tuya iot task */
    tuya_iot_start(&client);
    if (config_values.pairing_state == TUYA_BLE_PAIRING)
    {
        ESP_LOGI(TAG, "Tuya BLE pairing");
        tuya_reset();
        tuya_wifi_provisioning(&client, WIFI_PROVISIONING_MODE_BLE, &ble_token_get_cb);
    }

    while (1)
    {
        /* Loop to receive packets, and handles client keepalive */
        tuya_iot_yield(&client);
    }
}

void tuya_init()
{
    ESP_LOGI(TAG, "Tuya init");
    if (tuyaTaskHandle != NULL)
    {
        ESP_LOGI(TAG, "Tuya already init");
        tuya_iot_reset(&client);
        tuya_iot_start(&client);
        vTaskResume(tuyaTaskHandle);
        return;
    }
    xTaskCreate(tuya_link_app_task, "tuya_link", 1024 * 6, NULL, PRIORITY_TUYA, &tuyaTaskHandle);
}

void tuya_deinit()
{
    if (tuyaTaskHandle != NULL)
    {
        ESP_LOGI(TAG, "Tuya deinit");
        tuya_iot_stop(&client);
        tuya_iot_reset(&client);
        vTaskDelete(tuyaTaskHandle);
        tuyaTaskHandle = NULL;
        memset(&client, 0, sizeof(tuya_iot_client_t));
    }
}

static void tuya_send_callback(int result, void *user_data)
{
    uint8_t *sendComplete = (uint8_t *)user_data;
    *sendComplete = 1;
}

char *tuya_replace(char *str, str_replace_t *replace)
{
    for (str_replace_t *_replace = replace; replace->look != NULL && replace->replace != NULL; replace++)
    {
        if (_replace->look == NULL || _replace->replace == NULL)
        {
            continue;
        }
        ESP_LOGD(TAG, "Compare %s with %s", str, _replace->look);
        if (strstr(str, _replace->look) != NULL || strstr(_replace->look, str) != NULL)
        {
            ESP_LOGD(TAG, "Replace %s with %s", str, _replace->replace);
            str = (char *)_replace->replace;
            break;
        }
    }
    return str;
}

void tuya_fill_index(index_offset_t *out, linky_data_t *linky)
{
    switch (linky_mode)
    {
    case MODE_HIST:

        switch (linky_contract)
        {
        case C_BASE:
            break;
        case C_HC:
            out->index_hc = linky->hist.HCHC;
            out->index_hp = linky->hist.HCHP;
            break;
        case C_EJP:
            out->index_hc = linky->hist.EJPHN;
            out->index_hp = linky->hist.EJPHPM;
            break;
        case C_TEMPO:
            out->index_hc = linky->hist.BBRHCJB + linky->hist.BBRHCJW + linky->hist.BBRHCJR;
            out->index_hp = linky->hist.BBRHPJB + linky->hist.BBRHPJW + linky->hist.BBRHPJR;
            break;
        default:
            ESP_LOGE(TAG, "Unknown contract: %d", linky_contract);
            break;
        }
        out->index_total = linky->hist.TOTAL;
        break;
    case MODE_STD:
        switch (linky_contract)
        {
        case C_BASE:
            break;
        case C_HC:
        case C_EJP:
            out->index_hc = linky->std.EASF01;
            out->index_hp = linky->std.EASF02;
            break;
        case C_HEURES_SUPER_CREUSES:
            out->index_hc = linky->std.EASF01 + linky->std.EASF02;
            out->index_hp = linky->std.EASF03;
            break;
        case C_TEMPO:
        case C_SEM_WE_LUNDI:
        case C_SEM_WE_MERCREDI:
        case C_SEM_WE_VENDREDI:
        case C_ZEN_FLEX:
            out->index_total = linky->std.EASF01 + linky->std.EASF02 + linky->std.EASF03 + linky->std.EASF04 + linky->std.EASF05 + linky->std.EASF06 + linky->std.EASF07 + linky->std.EASF08 + linky->std.EASF09 + linky->std.EASF10;
            out->index_hc = linky->std.EASF01 + linky->std.EASF03 + linky->std.EASF05 + linky->std.EASF07 + linky->std.EASF09;
            out->index_hp = linky->std.EASF02 + linky->std.EASF04 + linky->std.EASF06 + linky->std.EASF08 + linky->std.EASF10;
            break;
        default:
            ESP_LOGE(TAG, "Unknown contract: %d", linky_contract);
            break;
        }
        out->index_total = linky->std.EAST;
        out->index_production = linky->std.EAIT;
        break;
    default:
        ESP_LOGE(TAG, "Unknown Linky mode: %d", linky_mode);
        break;
    }
}

uint32_t tuya_cap_value(uint64_t value_in)
{
    uint32_t value;
    if (value_in > INT32_MAX)
    {
        ESP_LOGW(TAG, "Tuya value capped: %llu", value_in);
        value = INT32_MAX;
    }
    else
    {
        value = (uint32_t)value_in;
    }
    return value;
}

/**
 * @brief Verify if the input is in the valid_values list
 *
 *
 * @param input input value
 * @param valid_values list of valid values, the first value is the unknown value
 * @return char*
 */
static char *tuya_verify_enum(char *input, const char *const *valid_values)
{
    for (char **valid = (char **)valid_values; *valid != NULL; valid++)
    {
        if (strcmp(input, *valid) == 0)
        {
            return *valid;
        }
    }

    ESP_LOGW(TAG, "Unknown value: %s", input);
    ESP_LOGW(TAG, "Valid values:");
    printf("\t[ ");
    for (char **valid = (char **)valid_values; *valid != NULL; valid++)
    {
        printf("\"%s\", ", *valid);
    }
    printf("]\n");
    return (char *)*valid_values;
}

uint8_t tuya_send_data(linky_data_t *linky)
{
    ESP_LOGI(TAG, "Send data to tuya");
    cJSON *jsonObject = cJSON_CreateObject(); // Create the root object

    // add index:
    index_offset_t now = {0};
    tuya_fill_index(&now, linky);
    now.index_total -= config_values.index_offset.index_total;
    now.index_hc -= config_values.index_offset.index_hc;
    now.index_hp -= config_values.index_offset.index_hp;
    now.index_production -= config_values.index_offset.index_production;

    switch (linky_contract)
    {
    case C_BASE:
    {
        cJSON_AddNumberToObject(jsonObject, "111", tuya_cap_value(now.index_total));
    }

    break;
    case C_HC:
    case C_EJP:
    case C_TEMPO:
    case C_SEM_WE_LUNDI:
    case C_SEM_WE_MERCREDI:
    case C_SEM_WE_VENDREDI:
    case C_ZEN_FLEX:
        cJSON_AddNumberToObject(jsonObject, "111", tuya_cap_value(now.index_total));
        cJSON_AddNumberToObject(jsonObject, "112", tuya_cap_value(now.index_hc));
        cJSON_AddNumberToObject(jsonObject, "113", tuya_cap_value(now.index_hp));
        break;

    default:
        ESP_LOGE(TAG, "Unknown contract: %d", linky_contract);
        break;
    }

    for (int i = 0; i < LinkyLabelListSize; i++)
    {
        if (LinkyLabelList[i].id < 101 || LinkyLabelList[i].id > 199)
        {
            continue; // dont send data for label < 101 : they are not used by tuya
        }
        if (LinkyLabelList[i].mode != linky_mode && LinkyLabelList[i].mode != ANY)
        {
            continue; // dont send data for label not used by current mode
        }
        // json
        char str_id[5];
        snprintf(str_id, sizeof(str_id), "%d", LinkyLabelList[i].id);
        switch (LinkyLabelList[i].id)
        {

        case 102:
            // Max power contract
            uint32_t max_power = *(uint32_t *)(LinkyLabelList[i].data);
            if (linky_mode == MODE_STD && linky_three_phase)
            {
                max_power *= 3;
            }

            cJSON_AddNumberToObject(jsonObject, "102", max_power);
            continue;
            break;

        case 103:
            uint16_t refresh_rate = *(uint16_t *)LinkyLabelList[i].data;
            if (refresh_rate > 300)
            {
                refresh_rate = 300;
            }

            if (refresh_rate < 30)
            {
                refresh_rate = 10;
            }

            cJSON_AddNumberToObject(jsonObject, "103", refresh_rate);
            continue;
            break;

        case 104:
            cJSON_AddNumberToObject(jsonObject, "104", tuya_cap_value(linky_data.uptime / 1000));
            continue;
            break;
        case 105:
            switch (linky_mode)
            {
            case MODE_HIST:
                cJSON_AddStringToObject(jsonObject, "105", "HISTORIQUE");
                break;
            case MODE_STD:
                cJSON_AddStringToObject(jsonObject, "105", "STANDARD");
                break;
            default:
                cJSON_AddStringToObject(jsonObject, "105", "INCONNU");
                break;
            }
            continue;
            break;
        case 106:
            if (linky_three_phase)
            {
                cJSON_AddStringToObject(jsonObject, "106", "TRIPHASE");
            }
            else
            {
                cJSON_AddStringToObject(jsonObject, "106", "MONOPHASE");
            }
            continue;
            break;
        case 107:
        {
            if (linky_contract >= C_COUNT)
            {
                linky_contract = C_UNKNOWN;
            }
            char *str = (char *)linky_tuya_str_contract[linky_contract];
            if (str == NULL)
            {
                str = (char *)linky_tuya_str_contract[C_UNKNOWN];
            }

            str = tuya_verify_enum(str, linky_tuya_str_contract);

            cJSON_AddStringToObject(jsonObject, "107", str);
            continue;
            break;
        }
        case 108:
        {
            char *str = (char *)LinkyLabelList[i].data;
            str = tuya_replace(str, str_current_tarif_replace);
            cJSON_AddStringToObject(jsonObject, "108", str);
            continue;
            break;
        }

        case 109:
        case 110:
        {
            char *value = (char *)LinkyLabelList[i].data;
            if (value == NULL || strlen(value) == 0)
                continue;

            char *str = tuya_verify_enum(value, linky_tuya_valid_color);
            if (str == NULL)
            {
                str = "INCONNU";
            }

            cJSON_AddStringToObject(jsonObject, str_id, str);
            continue;
            break;
        }
        default:
            break;
        }

        switch (LinkyLabelList[i].type)
        {
        case UINT8:
        {
            uint8_t *value = (uint8_t *)LinkyLabelList[i].data;
            if (value == NULL || *value == UINT8_MAX)
                continue;
            cJSON_AddNumberToObject(jsonObject, str_id, *value);
            break;
        }
        case UINT16:
        {
            uint16_t *value = (uint16_t *)LinkyLabelList[i].data;
            if (value == NULL || *value == UINT16_MAX)
                continue;
            cJSON_AddNumberToObject(jsonObject, str_id, *value);
            break;
        }
        case UINT32:
        {
            uint32_t *value = (uint32_t *)LinkyLabelList[i].data;
            if (value == NULL || *value == UINT32_MAX)
                continue;
            cJSON_AddNumberToObject(jsonObject, str_id, tuya_cap_value(*value));
            break;
        }
        case UINT32_TIME:
        {
            uint32_t *value = (uint32_t *)LinkyLabelList[i].data;
            if (value == NULL || *value == UINT32_MAX)
                continue;
            if (LinkyLabelList[i].device_class == ENERGY && *value == 0)
                continue;

            cJSON_AddNumberToObject(jsonObject, str_id, tuya_cap_value(*value));
            break;
        }
        case UINT64:
        {
            uint64_t *value = (uint64_t *)LinkyLabelList[i].data;
            if (value == NULL || *value == UINT64_MAX)
                continue;
            if (LinkyLabelList[i].device_class == ENERGY && *value == 0)
                continue;
            cJSON_AddNumberToObject(jsonObject, str_id, tuya_cap_value(*value));
            break;
        }
        case STRING:
        {
            char *value = (char *)LinkyLabelList[i].data;
            if (value == NULL)
                continue;
            uint32_t len = strlen(value);
            if (len == 0 || len > 255)
                continue;

            cJSON_AddStringToObject(jsonObject, str_id, value);
            break;
        }
        default:
            break;
        }
    }

    cJSON_AddStringToObject(jsonObject, "193", efuse_values.serial_number);

    char *json = cJSON_PrintUnformatted(jsonObject); // Convert the json object to string
    cJSON_Delete(jsonObject);                        // Delete the json object

    ESP_LOGI(TAG, "JSON: %s", json);
    uint8_t sendComplete = 0;
    time_t timout = MILLIS + 3000;

    tuya_iot_dp_report_json_with_notify(&client, json, NULL, tuya_send_callback, &sendComplete, 1000);
    while (sendComplete == 0 && MILLIS < timout)
    {
        vTaskDelay(10 / portTICK_PERIOD_MS);
    }
    free(json); // Free the memory

    if (sendComplete == 0)
    {
        ESP_LOGI(TAG, "Send data to tuya timeout");
        return 1;
    }
    ESP_LOGI(TAG, "Send data to tuya OK");
    return 0;
}

void tuya_reset()
{
    ESP_LOGI(TAG, "Reset Tuya");
    // config_values.pairing_state = TUYA_NOT_CONFIGURED;
    tuya_iot_activated_data_remove(&client);
}

void tuya_pairing_task(void *pvParameters)
{
    config_values.index_offset.value_saved = 0;
    config_values.pairing_state = TUYA_BLE_PAIRING;

    tuya_init();

    while (config_values.pairing_state != TUYA_WIFI_CONNECTING)
    {
        vTaskDelay(100 / portTICK_PERIOD_MS);
    }

    if (tuya_wait_event(TUYA_EVENT_MQTT_CONNECTED, 30000))
    {
        ESP_LOGI(TAG, "Tuya pairing failed: timeout, current state: %s", EVENT_ID2STR(lastEvent));
        tuya_stop();
        led_start_pattern(LED_SEND_FAILED);
        vTaskDelete(NULL);
    }

    config_values.pairing_state = TUYA_PAIRED;
    config_write();
    ESP_LOGI(TAG, "Tuya pairing: %d", config_values.pairing_state);
    led_start_pattern(LED_SEND_OK);
    esp_restart();
    vTaskDelete(NULL);
}
uint8_t tuya_wait_event(tuya_event_id_t event, uint32_t timeout)
{
    uint32_t timout = MILLIS + timeout;
    while (MILLIS < timout)
    {
        if (newEvent == 1)
        {
            newEvent = 0;
            if (lastEvent == event)
            {
                return 0;
            }
        }
        vTaskDelay(100 / portTICK_PERIOD_MS);
    }
    return 1;
}

uint8_t tuya_stop()
{
    ESP_LOGI(TAG, "Tuya stop");
    return tuya_iot_stop(&client);
}

uint8_t tuya_restart()
{
    ESP_LOGI(TAG, "Tuya restart");
    return tuya_iot_reconnect(&client);
}

// -------------------------------BLE -----------------------------------------

void ble_token_get_cb(wifi_info_t wifi_info)
{
    ESP_LOGI(TAG, "BLE token get callback");
    strncpy(config_values.ssid, (char *)wifi_info.ssid, sizeof(config_values.ssid));
    strncpy(config_values.password, (char *)wifi_info.pwd, sizeof(config_values.password));

    config_values.pairing_state = TUYA_WIFI_CONNECTING;

    if (gpio_led_pairing_task_handle != NULL)
    {
        vTaskDelete(gpio_led_pairing_task_handle);
        gpio_led_pairing_task_handle = NULL;
    }
    esp_err_t err = wifi_connect();
    if (err != ESP_OK)
    {
        ESP_LOGE(TAG, "Tuya pairing failed: no wifi");
        return;
    }
    ESP_LOGI(TAG, "Tuya pairing OK");
    return;
}

uint8_t tuya_available()
{
    return !(strnlen(config_values.tuya.device_uuid, sizeof(config_values.tuya.device_uuid)) == 0 || strnlen(config_values.tuya.device_auth, sizeof(config_values.tuya.device_auth)) == 0);
}