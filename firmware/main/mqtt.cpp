
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
#include <ArduinoJson.h>
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
    DynamicJsonDocument device(1024);
    const esp_app_desc_t *app_desc = esp_app_get_description();

    device["identifiers"] = MQTT_ID;
    device["name"] = MQTT_NAME;
    device["model"] = app_desc->project_name;
    device["manufacturer"] = "GammaTroniques";
    device["sw_version"] = app_desc->version;

    DynamicJsonDocument sensorConfig(1024);
    sensorConfig["~"] = config.values.mqtt.topic;
    sensorConfig["name"] = sensor.name;
    sensorConfig["unique_id"] = sensor.label;
    sensorConfig["object_id"] = sensor.label;

    char state_topic[100];
    snprintf(state_topic, sizeof(100), "~/%s", sensor.label);
    char type[50] = "sensor";
    if (sensor.type == HA_NUMBER)
    {
        snprintf(type, sizeof(type), "number");
    }
    snprintf(config_topic, 100, "homeassistant/%s/%s/%s/config", type, MQTT_ID, sensor.label);

    if (sensor.type == HA_NUMBER)
    {
        sensorConfig["command_topic"] = state_topic;
        sensorConfig["mode"] = "box";
        sensorConfig["min"] = 30;
        sensorConfig["max"] = 3600;
        sensorConfig["retain"] = "true";
        sensorConfig["qos"] = 2;
    }
    else
    {
        sensorConfig["state_topic"] = state_topic;
    }
    if (sensor.device_class == TIMESTAMP)
        sensorConfig["value_template"] = "{{ as_datetime(value) }}";
    if (sensor.device_class != NONE_CLASS)
        sensorConfig["device_class"] = HADeviceClassStr[sensor.device_class];
    if (strlen(sensor.icon) > 0)
        sensorConfig["icon"] = sensor.icon;
    switch (sensor.device_class)
    {
    case CURRENT:
        sensorConfig["unit_of_measurement"] = "A";
        break;
    case POWER_VA:
        sensorConfig["unit_of_measurement"] = "VA";
        break;
    case POWER_kVA:
        sensorConfig["unit_of_measurement"] = "kVA";
        break;
    case POWER_W:
        sensorConfig["unit_of_measurement"] = "W";
        break;
    case POWER_Q:
        sensorConfig["unit_of_measurement"] = "VAr";
        break;
    case ENERGY:
        sensorConfig["unit_of_measurement"] = "Wh";
        break;
    case ENERGY_Q:
        sensorConfig["unit_of_measurement"] = "VArh";
        break;
    case TIMESTAMP:
        break;
    case TENSION:
        sensorConfig["unit_of_measurement"] = "V";
        break;
    default:
        break;
    }

    if (sensor.realTime == REAL_TIME)
        sensorConfig["expire_after"] = config.values.refreshRate * 4;
    sensorConfig["device"] = device;
    serializeJson(sensorConfig, json, 1024);
}

static void mqtt_send_ha(LinkyData *linkydata)
{
    static uint8_t HAConfigured = 0;
    vTaskDelay(500 / portTICK_PERIOD_MS);
    if (config.values.mode == MODE_MQTT_HA && HAConfigured == 0)
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
            continue;
        snprintf(topic, sizeof(topic), "%s/%s", config.values.mqtt.topic, (char *)LinkyLabelList[i].label);
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
            snprintf(topic, sizeof(topic), "%s/%s", config.values.mqtt.topic, (char *)LinkyLabelList[i].label);
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
        esp_mqtt_client_publish(mqtt_client, topic, strValue, 0, 2, 0);
    }

    while ((mqtt_send_timeout > MILLIS) && mqtt_send_count < sensorsCount)
    {
        vTaskDelay(100 / portTICK_PERIOD_MS);
    }
    if (mqtt_send_count < sensorsCount)
    {
        ESP_LOGE(TAG, "Send Timeout");
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
    if (!mqtt_connected)
    {
        ESP_LOGI(TAG, "MQTT not connected, skipping Home Assistant Discovery");
        return;
    }

    char mqttBuffer[1024];
    char config_topic[100];

    for (int i = 0; i < LinkyLabelListSize; i++)
    {
        if (LinkyLabelList[i].mode != linky_mode && LinkyLabelList[i].mode != ANY)
            continue;
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
        mqtt_connected = 1;
        break;
    case MQTT_EVENT_DISCONNECTED:
        mqtt_connected = 0;
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
            if (config.values.refreshRate != refreshRate)
            {
                config.values.refreshRate = refreshRate;
                ESP_LOGI(TAG, "New RefreshRate = %d", config.values.refreshRate);
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
        break;
    default:
        // ESP_LOGI(TAG, "Other event id:%d", event->event_id);
        break;
    }
}

void mqtt_app_start(void)
{
    esp_log_level_set("mqtt_client", ESP_LOG_WARN);
    if (wifi_connected == 0)
    {
        ESP_LOGI(TAG, "WIFI not connected: MQTT ERROR");
        return;
    }

    if (mqtt_client != NULL)
    {
        ESP_LOGI(TAG, "MQTT already started");
        esp_mqtt_client_reconnect(mqtt_client);
        return;
    }
    char uri[200];
    esp_mqtt_client_config_t mqtt_cfg = {};

    // HOME ASSISTANT / MQTT
    snprintf(uri, sizeof(uri), "mqtt://%s:%d", config.values.mqtt.host, config.values.mqtt.port);
    mqtt_cfg.credentials.username = config.values.mqtt.username;
    mqtt_cfg.credentials.authentication.password = config.values.mqtt.password;
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
}

void mqtt_send(LinkyData *linky)
{
    if (wifi_connected == 0)
    {
        ESP_LOGI(TAG, "WIFI not connected: MQTT ERROR");
        return;
    }
    wifi_sending = 1;
    xTaskCreate(gpio_led_task_sending, "sendingLedTask", 2048, NULL, 1, NULL);
    if (!mqtt_connected)
    {
        mqtt_app_start();
    }
    switch (config.values.mode)
    {
    case MODE_MQTT:
    case MODE_MQTT_HA:
        if (strlen(config.values.mqtt.host) == 0 || config.values.mqtt.port == 0)
        {
            ESP_LOGI(TAG, "MQTT host not set: MQTT ERROR");
            return;
        }
        mqtt_send_ha(linky);
        break;
    default:
        return;
    }
    mqtt_stop();
    mqtt_connected = false;
    wifi_sending = 0;
}

void mqtt_stop()
{
    if (mqtt_client != NULL)
    {
        esp_mqtt_client_disconnect(mqtt_client);
        esp_mqtt_client_unregister_event(mqtt_client, (esp_mqtt_event_id_t)ESP_EVENT_ANY_ID, mqtt_event_handler);
        // esp_mqtt_client_stop(mqtt_client);
        esp_mqtt_client_destroy(mqtt_client);
        mqtt_client = NULL;
        vTaskDelay(1000 / portTICK_PERIOD_MS);
    }
}