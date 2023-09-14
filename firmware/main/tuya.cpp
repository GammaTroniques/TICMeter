#include "tuya.h"
#include "config.h"

#include "tuya_log.h"
#include "tuya_iot.h"
#include "cJSON.h"
#include "qrcode.h"

#define TAG "TUYA"
#define TUYA_PRODUCT_KEY "unwxvj8rhwjn1yvh" // for test
#define TUYA_DEVICE_UUID "uuid69ce06fcabad0183"
#define TUYA_DEVICE_AUTHKEY "fkXiV5eVSM9bB4r2OC1K7XYG7fBLMGTu"
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
        tuya_iot_dp_download(client, (const char *)event->value.asString);
        const char int_value[] = {"{\"104\":123}"};
        tuya_iot_dp_report_json(client, int_value);
        ESP_LOGI(TAG, "Repport");
        break;
    }

    default:
        break;
    }
}

static void tuya_link_app_task(void *pvParameters)
{
    int ret = OPRT_OK;
    const tuya_iot_config_t tuya_config = {
        .productkey = TUYA_PRODUCT_KEY,
        .uuid = TUYA_DEVICE_UUID,
        .authkey = TUYA_DEVICE_AUTHKEY,
        .software_ver = "1.0.0",
        .storage_namespace = "tuya_kv",
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
        vTaskDelay(100 / portTICK_PERIOD_MS);
    }
}

void init_tuya()
{
    ESP_LOGI(TAG, "Tuya init");
    xTaskCreate(tuya_link_app_task, "tuya_link", 1024 * 6, NULL, 4, &tuyaTaskHandle);
}

uint8_t send_tuya_data(LinkyData *linky)
{
    ESP_LOGI(TAG, "Send data to tuya");

    for (int i = 0; i < LinkyLabelListSize; i++)
    {
        // ESP_LOGI(TAG, "%s: %f", LinkyLabelList[i], linky->data[i]);
    }

    const char int_value[] = {"{\"104\":123}"};
    tuya_iot_dp_report_json(&client, int_value);
    return 0;
}