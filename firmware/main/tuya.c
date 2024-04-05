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

str_replace_t str_current_tarif_replace[] = {
    {"TH..", "BASE"},
    {"HC..", "Heures Creuses"},
    {"HP..", "Heures Pleines"},
};

/*==============================================================================
Public Variable
===============================================================================*/
TaskHandle_t tuyaTaskHandle = NULL;
TaskHandle_t tuya_ble_pairing_task_handle = NULL;

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

    // /* Process dp data */
    // cJSON *switch_obj = cJSON_GetObjectItem(dps, SWITCH_DP_ID_KEY);
    // if (cJSON_IsTrue(switch_obj))
    // {
    //     hardware_switch_set(true);
    // }
    // else if (cJSON_IsFalse(switch_obj))
    // {
    //     hardware_switch_set(false);
    // }

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
        ESP_LOGI(TAG, "Erasing NVS");
        esp_err_t err = nvs_flash_erase();
        if (err != ESP_OK)
        {
            ESP_LOGE(TAG, "Error (%s) erasing NVS!", esp_err_to_name(err));
        }
        ESP_LOGI(TAG, "Saving config");
        config_begin();
        config_write();
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
        break;

    case TUYA_OTA_EVENT_ON_DATA:
        ESP_LOGI(TAG, "OTA data len: %d %d/%d", event->data_len, event->offset, event->file_size);
        ret = ota_zlib_write(event->data, event->data_len);
        if (ret != ESP_OK)
        {
            ESP_LOGE(TAG, "OTA write error: %d", ret);
            tuya_ota_upgrade_status_report(handle, TUS_DOWNLOAD_ERROR_UNKONW);
        }
        break;

    case TUYA_OTA_EVENT_FINISH:
        ESP_LOGI(TAG, "OTA finish");
        ret = ota_zlib_end();
        break;
    case TUYA_OTA_EVENT_FAULT:
        ESP_LOGE(TAG, "OTA fault");
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
        // vTaskDelay(100 / portTICK_PERIOD_MS);
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

uint8_t tuya_compute_offset(linky_data_t *linky)
{
    ESP_LOGI(TAG, "Compute offset for tuya");
    if (config_values.index_offset.value_saved == 0)
    {
        ESP_LOGI(TAG, "Index offset not saved, skip tuya offset");
        return 1;
    }
    switch (linky_mode)
    {
    case MODE_HIST:
    {

        uint64_t *values[] = {&linky->hist.TOTAL, &linky->hist.HCHP, &linky->hist.EJPHN, &linky->hist.EJPHPM, &linky->hist.BBRHCJB, &linky->hist.BBRHPJB, &linky->hist.BBRHCJW, &linky->hist.BBRHPJW, &linky->hist.BBRHCJR, &linky->hist.BBRHPJR};
        if (linky->hist.BASE != UINT64_MAX)
        {
            linky->hist.BASE -= config_values.index_offset.index_01;
        }
        else
        {
            linky->hist.HCHC -= config_values.index_offset.index_01;
        }

        uint64_t *index = &config_values.index_offset.index_02;
        for (int i = 0; i < sizeof(values) / sizeof(values[0]); i++)
        {
            if (*values[i] == UINT64_MAX)
            {
                continue;
            }
            *values[i] -= *index;
            index++;
        }
    }
    break;
    case MODE_STD:
    {

        uint64_t *values[] = {&linky->std.EAST, &linky->std.EASF01, &linky->std.EASF02, &linky->std.EASF03, &linky->std.EASF04, &linky->std.EASF05, &linky->std.EASF06, &linky->std.EASF07, &linky->std.EASF08, &linky->std.EASF09, &linky->std.EASF10};
        uint64_t *index = &config_values.index_offset.index_total;
        for (int i = 0; i < sizeof(values) / sizeof(values[0]); i++)
        {
            if (*values[i] == UINT32_MAX)
            {
                continue;
            }
            *values[i] -= *index;
            index++;
        }
        break;
    }
    default:
        ESP_LOGE(TAG, "Unknown Linky mode: %d", linky_mode);
        return 1;
    }
    return 0;
}

uint8_t tuya_send_data(linky_data_t *linky)
{
    // tuya_iot_reconnect(&client);
    ESP_LOGI(TAG, "Send data to tuya");
    // DynamicJsonDocument device(1024);
    cJSON *jsonObject = cJSON_CreateObject(); // Create the root object

    if (config_values.index_offset.value_saved == 0)
    {
        ESP_LOGI(TAG, "Index offset not saved, skip tuya saving...");

        switch (linky_mode)
        {
        case MODE_HIST:
            if (linky->hist.BASE != UINT64_MAX)
            {
                config_values.index_offset.index_01 = linky->hist.BASE;
            }
            else
            {
                config_values.index_offset.index_01 = linky->hist.HCHC;
            }
            config_values.index_offset.index_02 = linky->hist.HCHP;
            config_values.index_offset.index_03 = linky->hist.EJPHN;
            config_values.index_offset.index_04 = linky->hist.EJPHPM;
            config_values.index_offset.index_05 = linky->hist.BBRHCJB;
            config_values.index_offset.index_06 = linky->hist.BBRHPJB;
            config_values.index_offset.index_07 = linky->hist.BBRHCJW;
            config_values.index_offset.index_08 = linky->hist.BBRHPJW;
            config_values.index_offset.index_09 = linky->hist.BBRHCJR;
            config_values.index_offset.index_10 = linky->hist.BBRHPJR;

            break;
        case MODE_STD:
            config_values.index_offset.index_01 = linky->std.EASF01;
            config_values.index_offset.index_02 = linky->std.EASF02;
            config_values.index_offset.index_03 = linky->std.EASF03;
            config_values.index_offset.index_04 = linky->std.EASF04;
            config_values.index_offset.index_05 = linky->std.EASF05;
            config_values.index_offset.index_06 = linky->std.EASF06;
            config_values.index_offset.index_07 = linky->std.EASF07;
            config_values.index_offset.index_08 = linky->std.EASF08;
            config_values.index_offset.index_09 = linky->std.EASF09;
            config_values.index_offset.index_10 = linky->std.EASF10;
            break;
        default:
            ESP_LOGE(TAG, "Unknown Linky mode: %d", linky_mode);
            break;
        }
        config_values.index_offset.value_saved = 1;
        config_write();
    }

    if (tuya_compute_offset(linky))
    {
        ESP_LOGE(TAG, "Error computing offset for tuya, skip sending data");
        return 1;
    }

    for (int i = 0; i < LinkyLabelListSize; i++)
    {
        if (LinkyLabelList[i].id < 101)
        {
            continue; // dont send data for label < 101 : they are not used by tuya
        }
        if (LinkyLabelList[i].mode != linky_mode && LinkyLabelList[i].mode != ANY)
        {
            continue; // dont send data for label not used by current mode
        }

        switch (LinkyLabelList[i].id)
        {
        case 105:
            switch (linky_mode)
            {
            case MODE_HIST:
                cJSON_AddStringToObject(jsonObject, "105", "Historique");
                break;
            case MODE_STD:
                cJSON_AddStringToObject(jsonObject, "105", "Standard");
                break;
            default:
                cJSON_AddStringToObject(jsonObject, "105", "Unknown");
                break;
            }
            continue;
            break;
        case 106:
            if (linky_three_phase)
            {
                cJSON_AddStringToObject(jsonObject, "106", "Triphase");
            }
            else
            {
                cJSON_AddStringToObject(jsonObject, "106", "Monophase");
            }
            continue;
            break;
        case 107:
        {
            char *str = (char *)linky_tuya_str_contract[linky_contract];
            if (str == NULL)
            {
                str = (char *)linky_tuya_str_contract[C_UNKNOWN];
            }
            cJSON_AddStringToObject(jsonObject, "107", str);
            break;
        }
        case 108:
        {
            char *str = (char *)LinkyLabelList[i].data;
            for (int i = 0; i < sizeof(str_current_tarif_replace) / sizeof(str_current_tarif_replace[0]); i++)
            {
                if (str_current_tarif_replace[i].look == NULL || str_current_tarif_replace[i].replace == NULL)
                {
                    continue;
                }
                if (strstr(str, str_current_tarif_replace[i].look) != NULL)
                {
                    str = (char *)str_current_tarif_replace[i].replace;
                    break;
                }
            }
            cJSON_AddStringToObject(jsonObject, "108", str);

            continue;
            break;
        }
        case 201:
        {

            switch (linky_contract)
            {
            case C_BASE:
                cJSON_AddNumberToObject(jsonObject, "110", *(uint32_t *)LinkyLabelList[i].data);
                break;
            case C_HC:
                cJSON_AddNumberToObject(jsonObject, "111", *(uint32_t *)LinkyLabelList[i].data);
                break;
            case C_EJP:
                cJSON_AddNumberToObject(jsonObject, "113", *(uint32_t *)LinkyLabelList[i].data);
                break;
            case C_TEMPO:
                cJSON_AddNumberToObject(jsonObject, "115", *(uint32_t *)LinkyLabelList[i].data);
                break;
            default:
                ESP_LOGE(TAG, "ID: %d, Unknown contract: %d", LinkyLabelList[i].id, linky_contract);
                break;
            }
            continue;
            break;
        }
        case 202:
        {
            switch (linky_contract)
            {
            case C_HC:
                cJSON_AddNumberToObject(jsonObject, "112", *(uint32_t *)LinkyLabelList[i].data);
                break;
            case C_EJP:
                cJSON_AddNumberToObject(jsonObject, "114", *(uint32_t *)LinkyLabelList[i].data);
                break;
            case C_TEMPO:
                cJSON_AddNumberToObject(jsonObject, "116", *(uint32_t *)LinkyLabelList[i].data);
                break;
            default:
                ESP_LOGE(TAG, "ID: %d, Unknown contract: %d", LinkyLabelList[i].id, linky_contract);
                break;
            }
            continue;
            break;
        }

        case 203:
        {
            switch (linky_contract)
            {
            case C_TEMPO:
                cJSON_AddNumberToObject(jsonObject, "117", *(uint32_t *)LinkyLabelList[i].data);
                break;
            default:
                ESP_LOGE(TAG, "ID: %d, Unknown contract: %d", LinkyLabelList[i].id, linky_contract);
                break;
            }
            continue;
            break;
        }
        case 204:
        {
            switch (linky_contract)
            {
            case C_TEMPO:
                cJSON_AddNumberToObject(jsonObject, "118", *(uint32_t *)LinkyLabelList[i].data);
                break;
            default:
                ESP_LOGE(TAG, "ID: %d, Unknown contract: %d", LinkyLabelList[i].id, linky_contract);
                break;
            }
            continue;
            break;
        }
        case 205:
        {
            switch (linky_contract)
            {
            case C_TEMPO:
                cJSON_AddNumberToObject(jsonObject, "119", *(uint32_t *)LinkyLabelList[i].data);
                break;
            default:
                ESP_LOGE(TAG, "ID: %d, Unknown contract: %d", LinkyLabelList[i].id, linky_contract);
                break;
            }
            continue;
            break;
        }
        case 206:
        {
            switch (linky_contract)
            {
            case C_TEMPO:
                cJSON_AddNumberToObject(jsonObject, "120", *(uint32_t *)LinkyLabelList[i].data);
                break;
            default:
                ESP_LOGE(TAG, "ID: %d, Unknown contract: %d", LinkyLabelList[i].id, linky_contract);
                break;
            }
            continue;
            break;
        }

        default:
            break;
        }

        // json
        char strId[5];
        snprintf(strId, sizeof(strId), "%d", LinkyLabelList[i].id);

        switch (LinkyLabelList[i].type)
        {
        case UINT8:
        {
            uint8_t *value = (uint8_t *)LinkyLabelList[i].data;
            if (value == NULL || *value == UINT8_MAX)
                continue;
            cJSON_AddNumberToObject(jsonObject, strId, *value);
            break;
        }
        case UINT16:
        {
            uint16_t *value = (uint16_t *)LinkyLabelList[i].data;
            if (value == NULL || *value == UINT16_MAX)
                continue;
            cJSON_AddNumberToObject(jsonObject, strId, *value);
            break;
        }
        case UINT32:
        {
            uint32_t *value = (uint32_t *)LinkyLabelList[i].data;
            if (value == NULL || *value == UINT32_MAX)
                continue;
            cJSON_AddNumberToObject(jsonObject, strId, *value);
            break;
        }
        case UINT32_TIME:
        {
            uint32_t *value = (uint32_t *)LinkyLabelList[i].data;
            if (value == NULL || *value == UINT32_MAX)
                continue;
            cJSON_AddNumberToObject(jsonObject, strId, *value);
            break;
        }
        case UINT64:
        {
            uint64_t *value = (uint64_t *)LinkyLabelList[i].data;
            if (value == NULL || *value == UINT64_MAX)
                continue;
            cJSON_AddNumberToObject(jsonObject, strId, *value);
            break;
        }
        case STRING:
        {
            char *value = (char *)LinkyLabelList[i].data;
            if (value == NULL || strlen(value) == 0)
                continue;
            cJSON_AddStringToObject(jsonObject, strId, value);
            break;
        }
        default:
            break;
        }
    }

    char *json = cJSON_PrintUnformatted(jsonObject); // Convert the json object to string
    cJSON_Delete(jsonObject);                        // Delete the json object

    ESP_LOGI(TAG, "JSON: %s", json);
    uint8_t sendComplete = 0;
    time_t timout = MILLIS + 3000;

    int ret = tuya_ota_upgrade_status_report(&ota_handle, TUS_RD);
    ESP_LOGI(TAG, "tuya_ota_upgrade_status_report: %d", ret);
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