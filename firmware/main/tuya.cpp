#include "tuya.h"
#include "config.h"

#include "tuya_log.h"
#include "tuya_iot.h"
#include "cJSON.h"
#include "qrcode.h"
#include "ArduinoJson.h"
#include "esp_ota_ops.h"

#define TAG "TUYA"

tuya_iot_client_t client;
TaskHandle_t tuyaTaskHandle = NULL;

void example_qrcode_print(const char *productkey, const char *uuid)
{
    ESP_LOGI(TAG, "https://smartapp.tuya.com/s/p?p=%s&uuid=%s&v=2.0", productkey, uuid);

    char urlbuf[255];
    sprintf(urlbuf, "https://smartapp.tuya.com/s/p?p=%s&uuid=%s&v=2.0", productkey, uuid);
    qrcode_display(urlbuf);

    ESP_LOGI(TAG, "(Use this URL to generate a static QR code for the Tuya APP scan code binding)");
}

void tuya_iot_dp_download(tuya_iot_client_t *client, const char *json_dps)
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

/* Tuya SDK event callback */
static void user_event_handler_on(tuya_iot_client_t *client, tuya_event_msg_t *event)
{
    switch (event->id)
    {
    case TUYA_EVENT_BIND_START:
        example_qrcode_print(client->config.productkey, client->config.uuid);
        break;

    case TUYA_EVENT_MQTT_CONNECTED:
        ESP_LOGI(TAG, "Device MQTT Connected!");
        break;

    case TUYA_EVENT_DP_RECEIVE:
    {

        ESP_LOGI(TAG, "TUYA_EVENT_DP_RECEIVE");
        break;
    }

    default:
        break;
    }
}

static void tuya_link_app_task(void *pvParameters)
{
    int ret = OPRT_OK;
    const esp_app_desc_t *app_desc = esp_app_get_description();

    const tuya_iot_config_t tuya_config = {
        .productkey = config.values.tuya.productID,
        .uuid = config.values.tuya.deviceUUID,
        .authkey = config.values.tuya.deviceAuth,
        .software_ver = app_desc->version,
        .modules = NULL,
        .skill_param = NULL,
        .storage_namespace = "tuya_kv",
        .firmware_key = NULL,
        .event_handler = user_event_handler_on,
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

void init_tuya()
{
    ESP_LOGI(TAG, "Tuya init");
    xTaskCreate(tuya_link_app_task, "tuya_link", 1024 * 6, NULL, 4, &tuyaTaskHandle);
}

void send_cb(int result, void *user_data)
{
    uint8_t *sendComplete = (uint8_t *)user_data;
    *sendComplete = 1;
}

uint8_t send_tuya_data(LinkyData *linky)
{
    // tuya_iot_reconnect(&client);
    ESP_LOGI(TAG, "Send data to tuya");
    DynamicJsonDocument device(1024);

    for (int i = 0; i < LinkyLabelListSize; i++)
    {
        if (LinkyLabelList[i].id < 101)
        {
            continue; // dont send data for label < 101 : they are not used by tuya
        }

        // json
        char strId[5];
        sprintf(strId, "%d", LinkyLabelList[i].id);

        switch (LinkyLabelList[i].type)
        {
        case UINT8:
        {
            uint8_t *value = (uint8_t *)LinkyLabelList[i].data;
            if (value == NULL || *value == UINT8_MAX)
                continue;
            device[strId] = *value;
            break;
        }
        case UINT16:
        {
            uint16_t *value = (uint16_t *)LinkyLabelList[i].data;
            if (value == NULL || *value == UINT16_MAX)
                continue;
            device[strId] = *value;
            break;
        }
        case UINT32:
        {
            uint32_t *value = (uint32_t *)LinkyLabelList[i].data;
            if (value == NULL || *value == UINT32_MAX)
                continue;
            device[strId] = *value;
            break;
        }
        case UINT32_TIME:
        {
            uint32_t *value = (uint32_t *)LinkyLabelList[i].data;
            if (value == NULL || *value == UINT32_MAX)
                continue;
            device[strId] = *value;
            break;
        }
        case UINT64:
        {
            uint64_t *value = (uint64_t *)LinkyLabelList[i].data;
            if (value == NULL || *value == UINT64_MAX)
                continue;
            device[strId] = *value;
            break;
        }
        case STRING:
        {
            char *value = (char *)LinkyLabelList[i].data;
            if (value == NULL || strlen(value) == 0)
                continue;
            device[strId] = value;
            break;
        }
        default:
            break;
        }
    }
    device["120"] = MILLIS;

    char json[1024];
    serializeJson(device, json);
    ESP_LOGI(TAG, "JSON: %s", json);
    uint8_t sendComplete = 0;
    time_t timout = MILLIS + 3000;
    tuya_iot_dp_report_json_with_notify(&client, json, NULL, send_cb, &sendComplete, 1000);
    while (sendComplete == 0 && MILLIS < timout)
    {
        vTaskDelay(10 / portTICK_PERIOD_MS);
    }
    if (sendComplete == 0)
    {
        ESP_LOGI(TAG, "Send data to tuya timeout");
        return 0;
    }
    ESP_LOGI(TAG, "Send data to tuya OK");
    return 1;
}

void reset_tuya()
{
    ESP_LOGI(TAG, "Reset Tuya");
    config.values.tuya.binded = 0;
    tuya_iot_activated_data_remove(&client);
}

uint8_t tuya_waiting_bind()
{

    // 1. STATE_TOKEN_PENDING
    // 2. STATE_ACTIVATING
    // 3. STATE_MQTT_CONNECT_START
    // 4. STATE_MQTT_YIELD
    switch (client.state)
    {
    case STATE_TOKEN_PENDING:
    case STATE_ACTIVATING:
    case STATE_MQTT_CONNECT_START:
        return 1;
        break;
    case STATE_MQTT_YIELD:
        config.values.tuya.binded = 1;
        return 0;
        break;
    default:
        return 0;
    }
}