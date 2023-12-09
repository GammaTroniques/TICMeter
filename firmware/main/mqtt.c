
/**
 * @file mqtt.cpp
 * @author Dorian Benech
 * @brief
 * @version 1.0
 * @date 2023-10-11
 *
 * @copyright Copyright (c) 2023
 *
 */

/*==============================================================================
 Local Include
===============================================================================*/
#include "mqtt.h"
#include "wifi.h"
#include "gpio.h"
#include "cJSON.h"
#include "esp_ota_ops.h"
#include "mbedtls/md.h"

/*==============================================================================
 Local Define
===============================================================================*/
#define TAG "MQTT"
#define STATIC_VALUE 0
#define REAL_TIME 1

#define MQTT_NAME "TICMeter"
#define MQTT_ID "TICMeter"
#define MQTT_SEND_TIMEOUT 10000 // in ms
#define MANUFACTURER "GammaTroniques"
/*==============================================================================
 Local Macro
===============================================================================*/

/*==============================================================================
 Local Type
===============================================================================*/
typedef enum
{
    MQTT_DEINIT,
    MQTT_DISCONNETED,
    MQTT_CONNECTING,
    MQTT_CONNECTED,
    MQTT_FAILED
} mqtt_state_t;

/*==============================================================================
 Local Function Declaration
===============================================================================*/
static void log_error_if_nonzero(const char *message, int error_code);
static void mqtt_create_sensor(char *json, char *config_topic, LinkyGroup sensor);
void mqtt_setup_ha_discovery();
/*==============================================================================
Public Variable
===============================================================================*/
esp_mqtt_client_handle_t mqtt_client = NULL;

/*==============================================================================
 Local Variable
===============================================================================*/
static mqtt_state_t mqtt_state = MQTT_DEINIT;
static uint16_t mqtt_sent_count = 0;
static uint16_t mqtt_sensors_count = 0;

/*==============================================================================
Function Implementation
===============================================================================*/
static void log_error_if_nonzero(const char *message, int error_code)
{
    if (error_code != 0)
    {
        ESP_LOGE(TAG, "Last error %s: 0x%x", message, error_code);
    }
}

static void mqtt_create_sensor(char *json, char *config_topic, LinkyGroup sensor)
{
    const esp_app_desc_t *app_desc = esp_app_get_description();
    cJSON *jsonDevice = cJSON_CreateObject(); // Create the root object
    // cJSON_AddStringToObject(jsonDevice, "ids", MQTT_ID);
    cJSON_AddStringToObject(jsonDevice, "name", MQTT_NAME);
    cJSON_AddStringToObject(jsonDevice, "mdl", app_desc->project_name);
    cJSON_AddStringToObject(jsonDevice, "mf", MANUFACTURER);
    cJSON_AddStringToObject(jsonDevice, "sw", app_desc->version);
    // serial
    cJSON *sn = cJSON_CreateArray();
    cJSON *cns = cJSON_CreateArray();
    cJSON_AddItemToArray(sn, cJSON_CreateString("SN"));
    cJSON_AddItemToArray(sn, cJSON_CreateString(efuse_values.serialNumber));
    cJSON_AddItemToObject(cns, "", sn);
    cJSON_AddItemToObject(jsonDevice, "cns", cns);

    cJSON *sensorConfig = cJSON_CreateObject(); // Create the root object
    cJSON_AddStringToObject(sensorConfig, "~", config_values.mqtt.topic);
    cJSON_AddStringToObject(sensorConfig, "name", sensor.name);
    char unique_id[100];
    snprintf(unique_id, sizeof(unique_id), "TICMeter_%s_%s", efuse_values.macAddress + 6, sensor.label);
    cJSON_AddStringToObject(sensorConfig, "uniq_id", unique_id);
    cJSON_AddStringToObject(sensorConfig, "obj_id", unique_id);

    char state_topic[100];
    snprintf(state_topic, sizeof(state_topic), "~/%s", sensor.label);
    snprintf(config_topic, 100, "homeassistant/%s/%s/%s/config", ha_sensors_str[sensor.type], MQTT_ID, sensor.label);

    if (sensor.type == HA_NUMBER)
    {
        cJSON_AddStringToObject(sensorConfig, "cmd_t", state_topic);
        cJSON_AddStringToObject(sensorConfig, "mode", "box");
        cJSON_AddNumberToObject(sensorConfig, "min", 30);
        cJSON_AddNumberToObject(sensorConfig, "max", 3600);
        cJSON_AddStringToObject(sensorConfig, "ret", "true");
        cJSON_AddNumberToObject(sensorConfig, "qos", 2);
    }
    else
    {
        cJSON_AddStringToObject(sensorConfig, "stat_t", state_topic);
    }
    if (sensor.device_class == TIMESTAMP)
    {
        cJSON_AddStringToObject(sensorConfig, "val_tpl", "{{ as_datetime(value) }}");
    }
    if (strlen(HADeviceClassStr[sensor.device_class]) > 0)
    {
        cJSON_AddStringToObject(sensorConfig, "dev_cla", HADeviceClassStr[sensor.device_class]);
    }
    if (strlen(sensor.icon) > 0)
    {
        cJSON_AddStringToObject(sensorConfig, "icon", sensor.icon);
    }

    if (sensor.device_class != NONE_CLASS && sensor.device_class != TIMESTAMP)
    {
        cJSON_AddStringToObject(sensorConfig, "unit_of_meas", HAUnitsStr[sensor.device_class]);
    }

    if (sensor.realTime == REAL_TIME)
    {
        cJSON_AddNumberToObject(sensorConfig, "exp_aft", config_values.refreshRate * 4);
    }

    cJSON_AddItemToObject(sensorConfig, "dev", jsonDevice);
    char *jsonString = cJSON_PrintUnformatted(sensorConfig);
    strncpy(json, jsonString, 1024);
    free(jsonString);
    cJSON_Delete(sensorConfig);
}

uint8_t mqtt_prepare_publish(LinkyData *linkydata)
{
    mqtt_sensors_count = 0;
    mqtt_sent_count = 0;
    if (mqtt_state == MQTT_DEINIT)
    {
        ESP_LOGE(TAG, "Cant prepare data: MQTT not initialized");
        return 0;
    }

    static uint8_t HAConfigured = 0;
    uint8_t has_error = 0;
    if (config_values.mode == MODE_MQTT_HA && HAConfigured == 0)
    {
        mqtt_setup_ha_discovery(linkydata);
        HAConfigured = 1;
    }

    char topic[150];
    char strValue[20];

    ESP_LOGI(TAG, "Pre-send Outbox size: %d", esp_mqtt_client_get_outbox_size(mqtt_client));

    linkydata->timestamp = wifi_get_timestamp();
    for (int i = 0; i < LinkyLabelListSize; i++)
    {
        if (LinkyLabelList[i].mode != linky_mode && LinkyLabelList[i].mode != ANY)
        {
            continue;
        }

        if (LinkyLabelList[i].data == NULL)
        {
            continue;
        }

        snprintf(topic, sizeof(topic), "%s/%s", config_values.mqtt.topic, (char *)LinkyLabelList[i].label);
        switch (LinkyLabelList[i].type)
        {
        case UINT8:
        {
            uint8_t *value = (uint8_t *)LinkyLabelList[i].data;
            if (*value == UINT8_MAX)
                continue;
            snprintf(strValue, sizeof(strValue), "%d", *value);
            break;
        }
        case UINT16:
        {
            uint16_t *value = (uint16_t *)LinkyLabelList[i].data;
            if (*value == UINT16_MAX)
                continue;
            snprintf(strValue, sizeof(strValue), "%d", *value);
            break;
        }
        case UINT32:
        {
            uint32_t *value = (uint32_t *)LinkyLabelList[i].data;
            if (*value == UINT32_MAX)
                continue;
            snprintf(strValue, sizeof(strValue), "%ld", *value);
            break;
        }
        case UINT64:
        {
            uint64_t *value = (uint64_t *)LinkyLabelList[i].data;
            if (*value == UINT64_MAX)
                continue;
            snprintf(strValue, sizeof(strValue), "%lld", *value);
            break;
        }
        case STRING:
        {
            char *value = (char *)LinkyLabelList[i].data;
            if (strlen(value) == 0)
                continue;
            snprintf(strValue, sizeof(strValue), "%s", value);
            break;
        }
        case UINT32_TIME:
        {

            TimeLabel *timeLabel = (TimeLabel *)LinkyLabelList[i].data;
            if (timeLabel->value == UINT32_MAX)
                continue;
            snprintf(topic, sizeof(topic), "%s/%s", config_values.mqtt.topic, (char *)LinkyLabelList[i].label);
            snprintf(strValue, sizeof(strValue), "%lu", timeLabel->value);
            break;
        }
        case HA_NUMBER:
            continue;
            break;

        default:
            break;
        }
        mqtt_sensors_count++;
        ESP_LOGD(TAG, "Publishing to  %s = %s", topic, strValue);
        // esp_mqtt_client_publish(mqtt_client, topic, strValue, 0, 2, 0);
        int ret = esp_mqtt_client_enqueue(mqtt_client, topic, strValue, 0, 2, 0, true);
        if (ret == -1)
        {
            ESP_LOGE(TAG, "Error while enqueue: %d", ret);
            has_error = 1;
        }
        else if (ret == -2)
        {
            ESP_LOGE(TAG, "Outbox full: %d", ret);
            has_error = 1;
        }
        ESP_LOGD(TAG, "Outbox size filling: %d %s", esp_mqtt_client_get_outbox_size(mqtt_client), topic);
    }

    if (has_error)
    {
        return 0;
    }
    ESP_LOGI(TAG, "All data are in the outbox");
    return 1;
}

void mqtt_setup_ha_discovery()
{
    char mqttBuffer[1024];
    char config_topic[100];

    for (int i = 0; i < LinkyLabelListSize; i++)
    {
        if (LinkyLabelList[i].mode != linky_mode && LinkyLabelList[i].mode != ANY)
        {
            continue;
        }

        if (LinkyLabelList[i].data == NULL)
        {
            continue;
        }

        switch (LinkyLabelList[i].type)
        {
        case UINT8:
            if (*(uint8_t *)LinkyLabelList[i].data == UINT8_MAX)
                continue;
            ESP_LOGD(TAG, "Adding %s: value = %d", LinkyLabelList[i].label, *(uint8_t *)LinkyLabelList[i].data);
            break;
        case UINT16:
            if (*(uint16_t *)LinkyLabelList[i].data == UINT16_MAX)
                continue;
            ESP_LOGD(TAG, "Adding %s: value = %d", LinkyLabelList[i].label, *(uint16_t *)LinkyLabelList[i].data);
            break;
        case UINT32:
            if (*(uint32_t *)LinkyLabelList[i].data == UINT32_MAX)
                continue;
            ESP_LOGD(TAG, "Adding %s: value = %ld", LinkyLabelList[i].label, *(uint32_t *)LinkyLabelList[i].data);
            break;
        case UINT64:
            if (*(uint64_t *)LinkyLabelList[i].data == UINT64_MAX)
                continue;
            ESP_LOGD(TAG, "Adding %s: value = %lld", LinkyLabelList[i].label, *(uint64_t *)LinkyLabelList[i].data);
            break;
        case STRING:
            if (strlen((char *)LinkyLabelList[i].data) == 0)
                continue;
            ESP_LOGD(TAG, "Adding %s: value = %s", LinkyLabelList[i].label, (char *)LinkyLabelList[i].data);
            break;
        case UINT32_TIME:
            if (((TimeLabel *)LinkyLabelList[i].data)->value == UINT32_MAX)
                continue;
            ESP_LOGD(TAG, "Adding %s: value = %lu", LinkyLabelList[i].label, ((TimeLabel *)LinkyLabelList[i].data)->value);
            break;
        case HA_NUMBER:
            break;
        default:
            continue;
            break;
        }
        mqtt_create_sensor(mqttBuffer, config_topic, LinkyLabelList[i]);
        esp_mqtt_client_enqueue(mqtt_client, config_topic, mqttBuffer, 0, 2, 1, true);
        mqtt_sensors_count++;
    }
    ESP_LOGI(TAG, "Home Assistant Discovery done");
}

void mqtt_event_handler(void *handler_args, esp_event_base_t base, int32_t event_id, void *event_data)
{
    ESP_LOGD(TAG, "Event dispatched from event loop base=%s, event_id=%" PRIi32 "", base, event_id);
    esp_mqtt_event_handle_t event = (esp_mqtt_event_handle_t)event_data;
    // esp_mqtt_client_handle_t client = event->client;
    // int msg_id;
    switch ((esp_mqtt_event_id_t)event_id)
    {
    case MQTT_EVENT_CONNECTED:
        if (mqtt_state != MQTT_CONNECTED)
        {
            mqtt_state = MQTT_CONNECTED;
        }
        // subscribe to input topic
        for (int i = 0; i < LinkyLabelListSize; i++)
        {
            if (LinkyLabelList[i].type == HA_NUMBER)
            {
                char topic[150];
                snprintf(topic, sizeof(topic), "%s/%s", config_values.mqtt.topic, LinkyLabelList[i].label);
                esp_mqtt_client_subscribe(mqtt_client, topic, 1);
                ESP_LOGI(TAG, "Subscribing to %s", topic);
            }
        }
        break;
    case MQTT_EVENT_DISCONNECTED:
        if (mqtt_state != MQTT_DISCONNETED)
        {
            mqtt_state = MQTT_DISCONNETED;
        }
        break;

    case MQTT_EVENT_SUBSCRIBED:
        break;
    case MQTT_EVENT_UNSUBSCRIBED:
        ESP_LOGI(TAG, "MQTT_EVENT_UNSUBSCRIBED, msg_id=%d", event->msg_id);
        break;
    case MQTT_EVENT_PUBLISHED:
        mqtt_sent_count++;
        break;
    case MQTT_EVENT_DATA:
    {
        ESP_LOGI(TAG, "MQTT_EVENT_DATA: %.*s=%.*s", event->topic_len, event->topic, event->data_len, event->data);
        char fullname[50];
        char strValue[20];
        strncpy(fullname, event->topic, event->topic_len);
        fullname[event->topic_len] = '\0';
        strncpy(strValue, event->data, event->data_len);
        strValue[event->data_len] = '\0';
        if (strlen(fullname) == 0)
        {
            break;
        }
        char *name = fullname + strlen(config_values.mqtt.topic) + 1; // +1 for '/'
        for (int i = 0; i < LinkyLabelListSize; i++)
        {
            if (strcmp(LinkyLabelList[i].label, name) == 0)
            {
                if (LinkyLabelList[i].data == NULL)
                {
                    ESP_LOGE(TAG, "Null pointer for %s", name);
                    break;
                }

                switch (LinkyLabelList[i].type)
                {
                case UINT8:
                    *(uint8_t *)LinkyLabelList[i].data = atoi(strValue);
                    break;
                case UINT16:
                    *(uint16_t *)LinkyLabelList[i].data = atoi(strValue);
                    break;
                case UINT32:
                    *(uint32_t *)LinkyLabelList[i].data = atol(strValue);
                    break;
                case UINT64:
                    *(uint64_t *)LinkyLabelList[i].data = atoll(strValue);
                    break;
                case STRING:
                    strncpy((char *)LinkyLabelList[i].data, strValue, LinkyLabelList[i].size);
                    break;
                case UINT32_TIME:
                    ((TimeLabel *)LinkyLabelList[i].data)->value = atol(strValue);
                    break;
                case HA_NUMBER:
                    uint16_t value = atoi(strValue);
                    uint16_t *config = (uint16_t *)LinkyLabelList[i].data;
                    if (value != *config)
                    {
                        *config = value;
                        ESP_LOGI(TAG, "Set %s = %d", name, *config);
                        config_write();
                    }
                    break;
                default:
                    break;
                }
                break;
            }
        }
    }
    break;
    case MQTT_EVENT_ERROR:
        ESP_LOGI(TAG, "MQTT_EVENT_ERROR, msg_id=%d", event->msg_id);
        if (event->error_handle->error_type == MQTT_ERROR_TYPE_TCP_TRANSPORT)
        {
            log_error_if_nonzero("reported from esp-tls", event->error_handle->esp_tls_last_esp_err);
            log_error_if_nonzero("reported from tls stack", event->error_handle->esp_tls_stack_err);
            log_error_if_nonzero("captured as transport's socket errno", event->error_handle->esp_transport_sock_errno);
            ESP_LOGI(TAG, "Last errno string (%s)", strerror(event->error_handle->esp_transport_sock_errno));
        }
        mqtt_state = MQTT_FAILED;
        break;
    default:
        // ESP_LOGI(TAG, "Other event id:%d", event->event_id);
        break;
    }
}

int mqtt_init(void)
{
    esp_log_level_set("mqtt_client", ESP_LOG_WARN);
    if (wifi_state == WIFI_DISCONNECTED)
    {
        ESP_LOGI(TAG, "WIFI not connected: MQTT ERROR");
        return -1;
    }

    mqtt_state = MQTT_CONNECTING;
    char uri[200];
    snprintf(uri, sizeof(uri), "mqtt://%s:%d", config_values.mqtt.host, config_values.mqtt.port);
    esp_mqtt_client_config_t mqtt_cfg = {
        .session.message_retransmit_timeout = 500,
        .outbox.limit = 16 * 1024,
        .credentials.username = config_values.mqtt.username,
        .credentials.authentication.password = config_values.mqtt.password,
        .task.priority = 1,
        .broker.address.uri = uri,
    };
    mqtt_client = esp_mqtt_client_init(&mqtt_cfg);
    /* The last argument may be used to pass data to the event handler, in this example mqtt_event_handler */
    esp_mqtt_client_register_event(mqtt_client, (esp_mqtt_event_id_t)ESP_EVENT_ANY_ID, mqtt_event_handler, NULL);
    ESP_LOGI(TAG, "init done");
    return 1;
}

int mqtt_send()
{
    esp_err_t err = ESP_OK;
    if (wifi_state == WIFI_DISCONNECTED)
    {
        ESP_LOGE(TAG, "WIFI not connected: MQTT ERROR");
        goto error;
    }
    wifi_sending = 1;
    xTaskCreate(gpio_led_task_sending, "sendingLedTask", 4096, NULL, 1, NULL);

    if (mqtt_state == MQTT_FAILED)
    {
        ESP_LOGI(TAG, "MQTT ERROR");
        goto error;
    }

    if (mqtt_client == NULL)
    {
        ESP_LOGW(TAG, "MQTT not initialized, initializing...");
        mqtt_init();
    }

    err = esp_mqtt_client_start(mqtt_client);
    if (err != ESP_OK)
    {
        ESP_LOGE(TAG, "Start failed with 0x%x", err);
        goto error;
    }

    ESP_LOGI(TAG, "Waiting for MQTT send done, outbox size: %d", esp_mqtt_client_get_outbox_size(mqtt_client));
    time_t mqtt_send_timeout = MILLIS + MQTT_SEND_TIMEOUT;
    while ((mqtt_send_timeout > MILLIS) && /*mqtt_sent_count < mqtt_sensors_count*/ esp_mqtt_client_get_outbox_size(mqtt_client) > 0)
    {
        if (mqtt_state == MQTT_DISCONNETED || mqtt_state == MQTT_FAILED)
        {
            ESP_LOGE(TAG, "MQTT Exit: %d", mqtt_state);
            break;
        }
        vTaskDelay(100 / portTICK_PERIOD_MS);
        ESP_LOGD(TAG, "Outbox size: %d", esp_mqtt_client_get_outbox_size(mqtt_client));
    }

    if (mqtt_state == MQTT_FAILED)
    {
        ESP_LOGE(TAG, "Send Failed");
        goto error;
    }

    if (/*mqtt_sent_count < mqtt_sensors_count*/ esp_mqtt_client_get_outbox_size(mqtt_client) > 0)
    {
        ESP_LOGE(TAG, "Send Timeout: %d/%d", mqtt_sent_count, mqtt_sensors_count);
        goto error;
    }
    else
    {
        ESP_LOGI(TAG, "Send Done");
    }
    esp_mqtt_client_disconnect(mqtt_client);
    esp_mqtt_client_stop(mqtt_client);
    wifi_sending = 0;
    return 1;
error:
    esp_mqtt_client_disconnect(mqtt_client);
    esp_mqtt_client_stop(mqtt_client);
    wifi_sending = 0;
    return 0;
}
