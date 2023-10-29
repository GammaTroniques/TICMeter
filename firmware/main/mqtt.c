
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
// #include <ArduinoJson.h>
#include "cJSON.h"
#include "esp_ota_ops.h"
#include "mbedtls/md.h"

/*==============================================================================
 Local Define
===============================================================================*/
#define TAG "MQTT"
#define STATIC_VALUE 0
#define REAL_TIME 1

#define MQTT_NAME "Linky"
#define MQTT_ID "linky"

/*==============================================================================
 Local Macro
===============================================================================*/

/*==============================================================================
 Local Type
===============================================================================*/

/*==============================================================================
 Local Function Declaration
===============================================================================*/
static void log_error_if_nonzero(const char *message, int error_code);
static void mqtt_create_sensor(char *json, char *config_topic, LinkyGroup sensor);

/*==============================================================================
Public Variable
===============================================================================*/
esp_mqtt_client_handle_t mqtt_client = NULL;

/*==============================================================================
 Local Variable
===============================================================================*/
static uint32_t mqtt_connected = 0;
static uint16_t mqtt_send_count = 0;
static uint32_t mqtt_send_timeout = 0;
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
    cJSON_AddStringToObject(jsonDevice, "identifiers", MQTT_ID);
    cJSON_AddStringToObject(jsonDevice, "name", MQTT_NAME);
    cJSON_AddStringToObject(jsonDevice, "model", app_desc->project_name);
    cJSON_AddStringToObject(jsonDevice, "manufacturer", "GammaTroniques");
    cJSON_AddStringToObject(jsonDevice, "sw_version", app_desc->version);

    cJSON *sensorConfig = cJSON_CreateObject(); // Create the root object
    cJSON_AddStringToObject(sensorConfig, "~", config_values.mqtt.topic);
    cJSON_AddStringToObject(sensorConfig, "name", sensor.name);
    cJSON_AddStringToObject(sensorConfig, "unique_id", sensor.label);
    cJSON_AddStringToObject(sensorConfig, "object_id", sensor.label);

    char state_topic[100];
    snprintf(state_topic, sizeof(state_topic), "~/%s", sensor.label);
    char type[50] = "sensor";
    if (sensor.type == HA_NUMBER)
    {
        snprintf(type, sizeof(type), "number");
    }
    snprintf(config_topic, 100, "homeassistant/%s/%s/%s/config", type, MQTT_ID, sensor.label);

    if (sensor.type == HA_NUMBER)
    {
        cJSON_AddStringToObject(sensorConfig, "command_topic", state_topic);
        cJSON_AddStringToObject(sensorConfig, "mode", "box");
        cJSON_AddNumberToObject(sensorConfig, "min", 30);
        cJSON_AddNumberToObject(sensorConfig, "max", 3600);
        cJSON_AddStringToObject(sensorConfig, "retain", "true");
        cJSON_AddNumberToObject(sensorConfig, "qos", 2);
    }
    else
    {
        cJSON_AddStringToObject(sensorConfig, "state_topic", state_topic);
    }
    if (sensor.device_class == TIMESTAMP)
    {
        cJSON_AddStringToObject(sensorConfig, "value_template", "{{ as_datetime(value) }}");
    }
    if (sensor.device_class != NONE_CLASS)
    {
        cJSON_AddStringToObject(sensorConfig, "device_class", HADeviceClassStr[sensor.device_class]);
    }
    if (strlen(sensor.icon) > 0)
    {
        cJSON_AddStringToObject(sensorConfig, "icon", sensor.icon);
    }

    cJSON_AddStringToObject(sensorConfig, "unit_of_measurement", HAUnitsStr[sensor.device_class]);

    if (sensor.realTime == REAL_TIME)
    {
        cJSON_AddNumberToObject(sensorConfig, "expire_after", config_values.refreshRate * 4);
    }

    cJSON_AddItemToObject(sensorConfig, "device", jsonDevice);
    char *jsonString = cJSON_PrintUnformatted(sensorConfig);
    strncpy(json, jsonString, 1024);
    free(jsonString);
    cJSON_Delete(sensorConfig);
}

static void mqtt_send_ha(LinkyData *linkydata)
{
    static uint8_t HAConfigured = 0;
    if (config_values.mode == MODE_MQTT_HA && HAConfigured == 0)
    {
        mqtt_setup_ha_discovery();
        HAConfigured = 1;
    }

    char topic[150];
    char strValue[20];

    mqtt_send_timeout = MILLIS + 10000;
    mqtt_send_count = 0;
    linkydata->timestamp = wifi_get_timestamp();
    uint16_t sensorsCount = 0;
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
        sensorsCount++;
        ESP_LOGD(TAG, "Publishing to  %s = %s", topic, strValue);
        esp_mqtt_client_publish(mqtt_client, topic, strValue, 0, 2, 0);
    }

    while ((mqtt_send_timeout > MILLIS) && mqtt_send_count < sensorsCount)
    {
        ESP_LOGI(TAG, "mqtt_connected = %ld", mqtt_connected);
        if (mqtt_connected == -1)
        {
            break;
        }
        vTaskDelay(100 / portTICK_PERIOD_MS);
    }
    if (mqtt_send_count < sensorsCount)
    {
        ESP_LOGE(TAG, "Send Timeout");
        mqtt_connected = -1;
    }
    else
    {
        ESP_LOGI(TAG, "Send Done");
    }
}

void mqtt_setup_ha_discovery()
{
    if (wifi_connected == 0)
    {
        ESP_LOGI(TAG, "WIFI not connected: MQTT ERROR");
        return;
    }
    if (mqtt_connected != 1)
    {
        ESP_LOGI(TAG, "MQTT not connected, skipping Home Assistant Discovery");
        return;
    }

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
            break;
        case UINT16:
            if (*(uint16_t *)LinkyLabelList[i].data == UINT16_MAX)
                continue;
            break;
        case UINT32:
            if (*(uint32_t *)LinkyLabelList[i].data == UINT32_MAX)
                continue;
            break;
        case UINT64:
            if (*(uint64_t *)LinkyLabelList[i].data == UINT64_MAX)
                continue;
            break;
        case STRING:
            if (strlen((char *)LinkyLabelList[i].data) == 0)
                continue;
            break;
        case UINT32_TIME:
            if (((TimeLabel *)LinkyLabelList[i].data)->value == UINT32_MAX)
                continue;
            break;
        case HA_NUMBER:
            break;
        default:
            continue;
            break;
        }

        mqtt_create_sensor(mqttBuffer, config_topic, LinkyLabelList[i]);
        esp_mqtt_client_publish(mqtt_client, config_topic, mqttBuffer, 0, 2, 1);
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
        if (mqtt_connected != -1)
        {
            mqtt_connected = 1;
        }
        break;
    case MQTT_EVENT_DISCONNECTED:
        if (mqtt_connected != -1)
        {
            mqtt_connected = 0;
        }
        break;

    case MQTT_EVENT_SUBSCRIBED:
        break;
    case MQTT_EVENT_UNSUBSCRIBED:
        ESP_LOGI(TAG, "MQTT_EVENT_UNSUBSCRIBED, msg_id=%d", event->msg_id);
        break;
    case MQTT_EVENT_PUBLISHED:
        mqtt_send_count++;
        break;
    case MQTT_EVENT_DATA:
    {
        char topic[100] = {0};
        memcpy(topic, event->topic, event->topic_len); // copy to not const string (add \0)
        if (strcmp(topic, MQTT_ID "/Refresh") == 0)
        {
            uint16_t refreshRate = atoi(event->data);
            if (config_values.refreshRate != refreshRate)
            {
                config_values.refreshRate = refreshRate;
                ESP_LOGI(TAG, "New RefreshRate = %d", config_values.refreshRate);
            }
        }
        else
        {
            ESP_LOGI(TAG, "MQTT_EVENT_DATA");
            printf("TOPIC=%.*s\r\n", event->topic_len, event->topic);
            printf("DATA=%.*s\r\n", event->data_len, event->data);
        }
    }
    break;
    case MQTT_EVENT_ERROR:
        ESP_LOGI(TAG, "MQTT_EVENT_ERROR");
        if (event->error_handle->error_type == MQTT_ERROR_TYPE_TCP_TRANSPORT)
        {
            log_error_if_nonzero("reported from esp-tls", event->error_handle->esp_tls_last_esp_err);
            log_error_if_nonzero("reported from tls stack", event->error_handle->esp_tls_stack_err);
            log_error_if_nonzero("captured as transport's socket errno", event->error_handle->esp_transport_sock_errno);
            ESP_LOGI(TAG, "Last errno string (%s)", strerror(event->error_handle->esp_transport_sock_errno));
        }
        mqtt_connected = -1;
        break;
    default:
        // ESP_LOGI(TAG, "Other event id:%d", event->event_id);
        break;
    }
}

int mqtt_app_start(void)
{
    esp_log_level_set("mqtt_client", ESP_LOG_WARN);
    if (wifi_connected == 0)
    {
        ESP_LOGI(TAG, "WIFI not connected: MQTT ERROR");
        return -1;
    }

    if (mqtt_client != NULL)
    {
        ESP_LOGI(TAG, "MQTT already started");
        esp_mqtt_client_reconnect(mqtt_client);
        mqtt_connected = 1;
        return 1;
    }

    mqtt_connected = 0;
    char uri[200];
    esp_mqtt_client_config_t mqtt_cfg = {};

    // HOME ASSISTANT / MQTT
    snprintf(uri, sizeof(uri), "mqtt://%s:%d", config_values.mqtt.host, config_values.mqtt.port);
    mqtt_cfg.credentials.username = config_values.mqtt.username;
    mqtt_cfg.credentials.authentication.password = config_values.mqtt.password;
    mqtt_cfg.broker.address.uri = uri;

    mqtt_client = esp_mqtt_client_init(&mqtt_cfg);
    /* The last argument may be used to pass data to the event handler, in this example mqtt_event_handler */
    esp_mqtt_client_register_event(mqtt_client, (esp_mqtt_event_id_t)ESP_EVENT_ANY_ID, mqtt_event_handler, NULL);
    esp_mqtt_client_start(mqtt_client);

    // HOME ASSISTANT / MQTT
    for (int i = 0; i < LinkyLabelListSize; i++)
    {
        if (LinkyLabelList[i].type == HA_NUMBER)
        {
            char topic[100];
            snprintf(topic, sizeof(topic), MQTT_ID "/%s", LinkyLabelList[i].label);
            esp_mqtt_client_subscribe(mqtt_client, topic, 1);
        }
    }
    return 1;
}

int mqtt_send(LinkyData *linky)
{
    if (wifi_connected == 0)
    {
        ESP_LOGI(TAG, "WIFI not connected: MQTT ERROR");
        goto error;
    }
    wifi_sending = 1;
    xTaskCreate(gpio_led_task_sending, "sendingLedTask", 2048, NULL, 1, NULL);
    if (mqtt_connected != 1)
    {
        mqtt_app_start();
    }
    switch (config_values.mode)
    {
    case MODE_MQTT:
    case MODE_MQTT_HA:
        if (strlen(config_values.mqtt.host) == 0 || config_values.mqtt.port == 0)
        {
            ESP_LOGI(TAG, "MQTT host not set: MQTT ERROR");
            goto error;
        }
        mqtt_send_ha(linky);
        break;
    default:
        ESP_LOGI(TAG, "MQTT not configured: MQTT ERROR");
        goto error;

        break;
    }
    if (mqtt_connected == -1)
    {
        ESP_LOGI(TAG, "MQTT ERROR");
        goto error;
    }

    xTaskCreate(mqtt_stop_task, "mqttStopTask", 2048, NULL, 1, NULL);

    mqtt_connected = 0;
    wifi_sending = 0;
    return 1;
error:
    ESP_LOGI(TAG, "MQTT ERROR goto");
    wifi_sending = 0;
    xTaskCreate(mqtt_stop_task, "mqttStopTask", 2048, NULL, 1, NULL);
    ESP_LOGI(TAG, "MQTT ERROR return");

    return 0;
}

void mqtt_stop_task(void *pvParameters)
{
    if (mqtt_client != NULL)
    {
        ESP_LOGI(TAG, "disconnecting MQTT");
        esp_mqtt_client_disconnect(mqtt_client);
        ESP_LOGI(TAG, "unre MQTT");
        esp_mqtt_client_unregister_event(mqtt_client, (esp_mqtt_event_id_t)ESP_EVENT_ANY_ID, mqtt_event_handler);
        // esp_mqtt_client_stop(mqtt_client);
        ESP_LOGI(TAG, "destroy MQTT");
        esp_mqtt_client_destroy(mqtt_client);
        mqtt_client = NULL;
    }
    ESP_LOGI(TAG, "MQTT stopped");
    vTaskDelete(NULL);
}