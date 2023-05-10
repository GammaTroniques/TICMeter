
#include "mqtt.h"
#include "wifi.h"
#include <ArduinoJson.h>
#include "esp_ota_ops.h"
#define TAG "MQTT"

#define HOMEASSISTANT_SENSOR "homeassistant/sensor"
#define HOMEASSISTANT_BINARY_SENSOR "homeassistant/binary_sensor"

const char *topics[] = {"ADCO", "OPTARIF", "ISOUSC", "BASE", "HCHC", "HCHP", "PTEC", "IINST", "IMAX", "PAPP", "HHPHC", "MOTDETAT", "Timestamp"};
const char *deviceClass[] = {NULL, NULL, NULL, "energy", "energy", "energy", NULL, "current", "current", "power", NULL, NULL, "timestamp"};

uint32_t mqttConnected = 0;
esp_mqtt_client_handle_t mqttClient = NULL;

static void log_error_if_nonzero(const char *message, int error_code)
{
    if (error_code != 0)
    {
        ESP_LOGE(TAG, "Last error %s: 0x%x", message, error_code);
    }
}

static void mqtt_event_handler(void *handler_args, esp_event_base_t base, int32_t event_id, void *event_data)
{
    ESP_LOGD(TAG, "Event dispatched from event loop base=%s, event_id=%" PRIi32 "", base, event_id);
    esp_mqtt_event_handle_t event = (esp_mqtt_event_handle_t)event_data;
    esp_mqtt_client_handle_t client = event->client;
    int msg_id;
    switch ((esp_mqtt_event_id_t)event_id)
    {
    case MQTT_EVENT_CONNECTED:
        ESP_LOGI(TAG, "MQTT_EVENT_CONNECTED");
        mqttConnected = 1;
        // msg_id = esp_mqtt_client_publish(client, "/topic/qos1", "data_3", 0, 1, 0);
        // ESP_LOGI(TAG, "sent publish successful, msg_id=%d", msg_id);

        // msg_id = esp_mqtt_client_subscribe(client, "/topic/qos0", 0);
        // ESP_LOGI(TAG, "sent subscribe successful, msg_id=%d", msg_id);

        // msg_id = esp_mqtt_client_subscribe(client, "/topic/qos1", 1);
        // ESP_LOGI(TAG, "sent subscribe successful, msg_id=%d", msg_id);

        // msg_id = esp_mqtt_client_unsubscribe(client, "/topic/qos1");
        // ESP_LOGI(TAG, "sent unsubscribe successful, msg_id=%d", msg_id);
        break;
    case MQTT_EVENT_DISCONNECTED:
        ESP_LOGI(TAG, "MQTT_EVENT_DISCONNECTED");
        mqttConnected = 0;
        break;

    case MQTT_EVENT_SUBSCRIBED:
        ESP_LOGI(TAG, "MQTT_EVENT_SUBSCRIBED, msg_id=%d", event->msg_id);
        msg_id = esp_mqtt_client_publish(client, "/topic/qos0", "data", 0, 0, 0);
        ESP_LOGI(TAG, "sent publish successful, msg_id=%d", msg_id);
        break;
    case MQTT_EVENT_UNSUBSCRIBED:
        ESP_LOGI(TAG, "MQTT_EVENT_UNSUBSCRIBED, msg_id=%d", event->msg_id);
        break;
    case MQTT_EVENT_PUBLISHED:
        ESP_LOGI(TAG, "MQTT_EVENT_PUBLISHED, msg_id=%d", event->msg_id);
        break;
    case MQTT_EVENT_DATA:
        ESP_LOGI(TAG, "MQTT_EVENT_DATA");
        printf("TOPIC=%.*s\r\n", event->topic_len, event->topic);
        printf("DATA=%.*s\r\n", event->data_len, event->data);
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
        ESP_LOGI(TAG, "Other event id:%d", event->event_id);
        break;
    }
}

void mqtt_app_start(void)
{
    if (wifiConnected == 0)
    {
        ESP_LOGI(TAG, "WIFI not connected: MQTT ERROR");
        return;
    }

    if (mqttClient != NULL)
    {
        ESP_LOGI(TAG, "MQTT already started");
        return;
    }

    ESP_LOGI(TAG, "STARTING MQTT");
    char *uri = (char *)malloc(150);
    sprintf(uri, "mqtt://%s:%d", config.values.mqtt.host, config.values.mqtt.port);
    ESP_LOGI(TAG, "MQTT URI: %s", uri);

    esp_mqtt_client_config_t mqtt_cfg = {0};
    mqtt_cfg.broker.address.uri = uri;
    mqtt_cfg.credentials.username = "admin";
    mqtt_cfg.credentials.authentication.password = "hassio49";

    mqttClient = esp_mqtt_client_init(&mqtt_cfg);
    /* The last argument may be used to pass data to the event handler, in this example mqtt_event_handler */
    esp_mqtt_client_register_event(mqttClient, (esp_mqtt_event_id_t)ESP_EVENT_ANY_ID, mqtt_event_handler, NULL);
    esp_mqtt_client_start(mqttClient);

    // xTaskCreate(Publisher_Task, "Publisher_Task", 1024 * 5, NULL, 5, NULL);
    // ESP_LOGI(TAG, "MQTT Publisher_Task is up and running\n");
}

#define MQTT_NAME "Linky"
#define MQTT_ID "linky"

void createSensor(char *json, char *config_topic, const char *name, const char *entity_id, const char *device_class)
{

    DynamicJsonDocument device(1024);
    const esp_app_desc_t *app_desc = esp_ota_get_app_description();

    device["identifiers"] = MQTT_ID;
    device["name"] = MQTT_NAME;
    device["model"] = app_desc->project_name;
    device["manufacturer"] = "GammaTroniques";
    device["sw_version"] = app_desc->version;

    DynamicJsonDocument sensorConfig(1024);
    sensorConfig["~"] = config.values.mqtt.topic;
    sensorConfig["name"] = name;
    sensorConfig["unique_id"] = entity_id;
    sensorConfig["object_id"] = entity_id;

    char state_topic[100];
    sprintf(state_topic, "~/%s", entity_id);
    sprintf(config_topic, "%s/%s/%s/config", HOMEASSISTANT_SENSOR, MQTT_ID, entity_id);

    sensorConfig["state_topic"] = state_topic;
    if (device_class != NULL)
    {
        sensorConfig["device_class"] = device_class;
    }
    sensorConfig["device"] = device;
    serializeJson(sensorConfig, json, 1024);
}

void setupHomeAssistantDiscovery()
{
    if (wifiConnected == 0)
    {
        ESP_LOGI(TAG, "WIFI not connected: MQTT ERROR");
        return;
    }
    if (!mqttConnected)
    {
        ESP_LOGI(TAG, "MQTT not connected, skipping Home Assistant Discovery");
        return;
    }

    char mqttBuffer[1024];
    char config_topic[100];

    for (int i = 0; i < 13; i++)
    {
        createSensor(mqttBuffer, config_topic, topics[i], topics[i], deviceClass[i]);
        esp_mqtt_client_publish(mqttClient, config_topic, mqttBuffer, 0, 1, 0);
    }
}

void sendToMqtt(LinkyData *linky)
{
    if (wifiConnected == 0)
    {
        ESP_LOGI(TAG, "WIFI not connected: MQTT ERROR");
        return;
    }
    if (!mqttConnected)
    {
        ESP_LOGI(TAG, "MQTT not connected");
        mqtt_app_start();
    }

    char topic[150];
    char value[20];
    const void *values[] = {&linky->ADCO, &linky->OPTARIF, &linky->ISOUSC, &linky->BASE, &linky->HCHC, &linky->HCHP, &linky->PTEC, &linky->IINST, &linky->IMAX, &linky->PAPP, &linky->HHPHC, &linky->MOTDETAT, &linky->timestamp};

    for (int i = 0; i < 13; i++)
    {
        if (sizeof(values[i]) == sizeof(unsigned long))
        {
            sprintf(topic, "%s/%s", config.values.mqtt.topic, (char *)topics[i]);
            sprintf(value, "%lu", *(uint32_t *)(values[i]));
        }
        else
        {
            sprintf(topic, "%s/%s", config.values.mqtt.topic, (char *)topics[i]);
            sprintf(value, "%s", (char *)(values[i]));
        }
        esp_mqtt_client_publish(mqttClient, topic, value, 0, 1, 0);
    }
}