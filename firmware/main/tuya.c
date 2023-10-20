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
#include "cJSON.h"
#include "qrcode.h"
#include "wifi.h"
#include "esp_ota_ops.h"
/*==============================================================================
 Local Define
===============================================================================*/

#define TAG "TUYA"

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
/*==============================================================================
 Local Function Declaration
===============================================================================*/

static void tuya_qrcode_print(const char *productkey, const char *uuid);
static void tuya_user_event_handler_on(tuya_iot_client_t *client, tuya_event_msg_t *event);
static void tuya_iot_dp_download(tuya_iot_client_t *client, const char *json_dps);
static void tuya_link_app_task(void *pvParameters);
static void tuya_send_callback(int result, void *user_data);

/*==============================================================================
Public Variable
===============================================================================*/
TaskHandle_t tuyaTaskHandle = NULL;

/*==============================================================================
 Local Variable
===============================================================================*/
static tuya_iot_client_t client;
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

static void tuya_user_event_handler_on(tuya_iot_client_t *client, tuya_event_msg_t *event)
{
    ESP_LOGI(TAG, "TUYA_EVENT: %s", EVENT_ID2STR(event->id));
    switch (event->id)
    {
    case TUYA_EVENT_BIND_START:
        tuya_qrcode_print(client->config.productkey, client->config.uuid);
        break;

    case TUYA_EVENT_MQTT_CONNECTED:
        ESP_LOGI(TAG, "Device MQTT Connected!");
        break;

    case TUYA_EVENT_DP_RECEIVE:
    {
        ESP_LOGI(TAG, "TUYA_EVENT_DP_RECEIVE");
        break;
    }
    case TUYA_EVENT_RESET:
        ESP_LOGI(TAG, "Tuya unbined");
        config_values.tuyaBinded = 2; // want to be binded on next boot
        config_write();
        break;
    case TUYA_EVENT_BIND_TOKEN_ON:
        // start of binding

        break;
    case TUYA_EVENT_ACTIVATE_SUCCESSED:
        ESP_LOGI(TAG, "Tuya binded");
        config_values.tuyaBinded = 1; // binded
        config_write();
        break;
    default:
        break;
    }
    newEvent = 1;
    lastEvent = event->id;
}

static void tuya_link_app_task(void *pvParameters)
{
    int ret = OPRT_OK;
    const esp_app_desc_t *app_desc = esp_app_get_description();

    const tuya_iot_config_t tuya_config = {
        .productkey = config_values.tuyaKeys.productID,
        .uuid = config_values.tuyaKeys.deviceUUID,
        .authkey = config_values.tuyaKeys.deviceAuth,
        .software_ver = app_desc->version,
        .modules = NULL,
        .skill_param = NULL,
        .storage_namespace = "tuya_kv",
        .firmware_key = NULL,
        .event_handler = tuya_user_event_handler_on,
    };

    /* Initialize Tuya device configuration */
    ret = tuya_iot_init(&client, &tuya_config);

    assert(ret == OPRT_OK);

    /* Start tuya iot task */
    tuya_iot_start(&client);
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
    xTaskCreate(tuya_link_app_task, "tuya_link", 1024 * 6, NULL, 4, &tuyaTaskHandle);
}

static void tuya_send_callback(int result, void *user_data)
{
    uint8_t *sendComplete = (uint8_t *)user_data;
    *sendComplete = 1;
}

uint8_t tuya_send_data(LinkyData *linky)
{
    // tuya_iot_reconnect(&client);
    ESP_LOGI(TAG, "Send data to tuya");
    // DynamicJsonDocument device(1024);
    cJSON *jsonObject = cJSON_CreateObject(); // Create the root object

    for (int i = 0; i < LinkyLabelListSize; i++)
    {
        if (LinkyLabelList[i].id < 101)
        {
            continue; // dont send data for label < 101 : they are not used by tuya
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
            // device[strId] = *value;
            cJSON_AddNumberToObject(jsonObject, strId, *value);
            break;
        }
        case UINT16:
        {
            uint16_t *value = (uint16_t *)LinkyLabelList[i].data;
            if (value == NULL || *value == UINT16_MAX)
                continue;
            // device[strId] = *value;
            cJSON_AddNumberToObject(jsonObject, strId, *value);
            break;
        }
        case UINT32:
        {
            uint32_t *value = (uint32_t *)LinkyLabelList[i].data;
            if (value == NULL || *value == UINT32_MAX)
                continue;
            // device[strId] = *value;
            cJSON_AddNumberToObject(jsonObject, strId, *value);
            break;
        }
        case UINT32_TIME:
        {
            uint32_t *value = (uint32_t *)LinkyLabelList[i].data;
            if (value == NULL || *value == UINT32_MAX)
                continue;
            // device[strId] = *value;
            cJSON_AddNumberToObject(jsonObject, strId, *value);
            break;
        }
        case UINT64:
        {
            uint64_t *value = (uint64_t *)LinkyLabelList[i].data;
            if (value == NULL || *value == UINT64_MAX)
                continue;
            // device[strId] = *value;
            cJSON_AddNumberToObject(jsonObject, strId, *value);
            break;
        }
        case STRING:
        {
            char *value = (char *)LinkyLabelList[i].data;
            if (value == NULL || strlen(value) == 0)
                continue;
            // device[strId] = value;
            cJSON_AddStringToObject(jsonObject, strId, value);
            break;
        }
        default:
            break;
        }
    }
    cJSON_AddNumberToObject(jsonObject, "134", MILLIS / 1000 / 60);

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
    config_values.tuyaBinded = 0;
    tuya_iot_activated_data_remove(&client);
}

void tuya_pairing_task(void *pvParameters)
{
    ESP_LOGI(TAG, "Tuya pairing");
    if (!wifi_connect())
    {
        ESP_LOGE(TAG, "Tuya pairing failed: no wifi");
        vTaskDelete(NULL);
    }
    tuya_reset();
    tuya_init();
    while (config_values.tuyaBinded != 1)
    {
        vTaskDelay(100 / portTICK_PERIOD_MS);
    }
    ESP_LOGI(TAG, "Tuya pairing done");
    config_write();
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
        vTaskDelay(10 / portTICK_PERIOD_MS);
    }
    return 1;
}