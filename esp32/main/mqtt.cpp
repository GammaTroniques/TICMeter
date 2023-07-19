
#include "mqtt.h"
#include "wifi.h"
#include <ArduinoJson.h>
#include "esp_ota_ops.h"
#include "mbedtls/md.h"
#define TAG "MQTT"

#define TYPE_STRING 0
#define TYPE_UINT32 1
#define TYPE_UINT16 2

#define STATIC_VALUE 0
#define REAL_TIME 1

#define TYPE_SENSOR "sensor"
#define TYPE_NUMBER "number"

#define MQTT_NAME "Linky"
#define MQTT_ID "linky"
struct sensorConfig
{
    char type[32] = TYPE_SENSOR;
    char name[32] = "";
    char unique_id[32] = "";
    char device_class[32] = "";
    char unit_of_measurement[32] = "";
    char value_template[32] = "";
    uint8_t valueType = TYPE_STRING;
    uint8_t realTime = 0;
    char icon[32] = "";
};
#define member_size(type, member) sizeof(((type *)0)->member)

// clang-format off
struct sensorConfig sensors[] = {
    // TYPE          Name                         Unique ID              Device class   Unit        Value template           Value type  Real time
    {TYPE_SENSOR, "Identifiant",                "ADCO",                 "",             "",     "",                         TYPE_STRING, STATIC_VALUE,   "mdi:card-account-details" },
    {TYPE_SENSOR, "Option tarifaire",           "OPTARIF",              "",             "",     "",                         TYPE_STRING, STATIC_VALUE,   "mdi:cash-multiple"        },
    {TYPE_SENSOR, "Intensité souscrite",        "ISOUSC",               "current",      "A",    "",                         TYPE_UINT32, STATIC_VALUE,   ""                           },
    {TYPE_SENSOR, "Index Base",                 "BASE",                 "energy",       "Wh",   "",                         TYPE_UINT32, STATIC_VALUE,   ""                           },
    {TYPE_SENSOR, "Index Heures Creuses",       "HCHC",                 "energy",       "Wh",   "",                         TYPE_UINT32, STATIC_VALUE,   ""                           },
    {TYPE_SENSOR, "Index Heures Pleines",       "HCHP",                 "energy",       "Wh",   "",                         TYPE_UINT32, STATIC_VALUE,   ""                           },
    {TYPE_SENSOR, "Période tarifaire en cours", "PTEC",                 "",             "",     "",                         TYPE_STRING, STATIC_VALUE,   "mdi:calendar-clock"       },
    {TYPE_SENSOR, "Intensité instantanée",      "IINST",                "current",      "A",    "",                         TYPE_UINT32, REAL_TIME,      ""                           },
    {TYPE_SENSOR, "Intensité maximale",         "IMAX",                 "current",      "A",    "",                         TYPE_UINT32, STATIC_VALUE,   ""                           },
    {TYPE_SENSOR, "Puissance apparente",        "PAPP",                 "power",        "VA",   "",                         TYPE_UINT32, REAL_TIME,      ""                           },
    {TYPE_SENSOR, "Horaire HC",                 "HHPHC",                "",             "",     "",                         TYPE_STRING, STATIC_VALUE,   "mdi:home-clock"           },
    {TYPE_SENSOR, "Mot d'état du compteur",     "MOTDETAT",             "",             "",     "",                         TYPE_STRING, STATIC_VALUE,   "mdi:state-machine"        },
    {TYPE_SENSOR, "Timestamp",                  "Timestamp",            "timestamp",    "",     "{{ as_datetime(value) }}", TYPE_UINT32, STATIC_VALUE,   ""                           },
    {TYPE_SENSOR, "Refresh Rate",               "currentRefreshRate",   "",             "sec",  "",                         TYPE_UINT16, STATIC_VALUE,   "mdi:refresh"              },
    {TYPE_NUMBER, "Refresh Rate",               "RefreshRate",          "",             "sec",  "",                         TYPE_UINT32, STATIC_VALUE,   "mdi:refresh"              },
};
// clang-format on

uint32_t mqttConnected = 0;
esp_mqtt_client_handle_t mqttClient = NULL;

static void log_error_if_nonzero(const char *message, int error_code)
{
    if (error_code != 0)
    {
        ESP_LOGE(TAG, "Last error %s: 0x%x", message, error_code);
    }
}

uint16_t mqttSendCount = 0;
uint32_t mqttSendTimout = 0;

void mqtt_event_handler(void *handler_args, esp_event_base_t base, int32_t event_id, void *event_data)
{
    ESP_LOGD(TAG, "Event dispatched from event loop base=%s, event_id=%" PRIi32 "", base, event_id);
    esp_mqtt_event_handle_t event = (esp_mqtt_event_handle_t)event_data;
    // esp_mqtt_client_handle_t client = event->client;
    // int msg_id;
    switch ((esp_mqtt_event_id_t)event_id)
    {
    case MQTT_EVENT_CONNECTED:
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
        // ESP_LOGI(TAG, "MQTT_EVENT_DISCONNECTED");
        mqttConnected = 0;
        break;

    case MQTT_EVENT_SUBSCRIBED:
        // ESP_LOGI(TAG, "MQTT_EVENT_SUBSCRIBED, msg_id=%d", event->msg_id);
        break;
    case MQTT_EVENT_UNSUBSCRIBED:
        ESP_LOGI(TAG, "MQTT_EVENT_UNSUBSCRIBED, msg_id=%d", event->msg_id);
        break;
    case MQTT_EVENT_PUBLISHED:
        // ESP_LOGI(TAG, "MQTT_EVENT_PUBLISHED, msg_id=%d", event->msg_id);
        mqttSendCount++;
        break;
    case MQTT_EVENT_DATA:
    {
        char topic[100] = {0};
        memcpy(topic, event->topic, event->topic_len); // copy to not const string (add \0)
        if (strcmp(topic, MQTT_ID "/RefreshRate") == 0)
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
const char *tuyaMqttCert =
    "-----BEGIN CERTIFICATE-----\n"
    "MIIE0DCCA7igAwIBAgIBBzANBgkqhkiG9w0BAQsFADCBgzELMAkGA1UEBhMCVVMx\n"
    "EDAOBgNVBAgTB0FyaXpvbmExEzARBgNVBAcTClNjb3R0c2RhbGUxGjAYBgNVBAoT\n"
    "EUdvRGFkZHkuY29tLCBJbmMuMTEwLwYDVQQDEyhHbyBEYWRkeSBSb290IENlcnRp\n"
    "ZmljYXRlIEF1dGhvcml0eSAtIEcyMB4XDTExMDUwMzA3MDAwMFoXDTMxMDUwMzA3\n"
    "MDAwMFowgbQxCzAJBgNVBAYTAlVTMRAwDgYDVQQIEwdBcml6b25hMRMwEQYDVQQH\n"
    "EwpTY290dHNkYWxlMRowGAYDVQQKExFHb0RhZGR5LmNvbSwgSW5jLjEtMCsGA1UE\n"
    "CxMkaHR0cDovL2NlcnRzLmdvZGFkZHkuY29tL3JlcG9zaXRvcnkvMTMwMQYDVQQD\n"
    "EypHbyBEYWRkeSBTZWN1cmUgQ2VydGlmaWNhdGUgQXV0aG9yaXR5IC0gRzIwggEi\n"
    "MA0GCSqGSIb3DQEBAQUAA4IBDwAwggEKAoIBAQC54MsQ1K92vdSTYuswZLiBCGzD\n"
    "BNliF44v/z5lz4/OYuY8UhzaFkVLVat4a2ODYpDOD2lsmcgaFItMzEUz6ojcnqOv\n"
    "K/6AYZ15V8TPLvQ/MDxdR/yaFrzDN5ZBUY4RS1T4KL7QjL7wMDge87Am+GZHY23e\n"
    "cSZHjzhHU9FGHbTj3ADqRay9vHHZqm8A29vNMDp5T19MR/gd71vCxJ1gO7GyQ5HY\n"
    "pDNO6rPWJ0+tJYqlxvTV0KaudAVkV4i1RFXULSo6Pvi4vekyCgKUZMQWOlDxSq7n\n"
    "eTOvDCAHf+jfBDnCaQJsY1L6d8EbyHSHyLmTGFBUNUtpTrw700kuH9zB0lL7AgMB\n"
    "AAGjggEaMIIBFjAPBgNVHRMBAf8EBTADAQH/MA4GA1UdDwEB/wQEAwIBBjAdBgNV\n"
    "HQ4EFgQUQMK9J47MNIMwojPX+2yz8LQsgM4wHwYDVR0jBBgwFoAUOpqFBxBnKLbv\n"
    "9r0FQW4gwZTaD94wNAYIKwYBBQUHAQEEKDAmMCQGCCsGAQUFBzABhhhodHRwOi8v\n"
    "b2NzcC5nb2RhZGR5LmNvbS8wNQYDVR0fBC4wLDAqoCigJoYkaHR0cDovL2NybC5n\n"
    "b2RhZGR5LmNvbS9nZHJvb3QtZzIuY3JsMEYGA1UdIAQ/MD0wOwYEVR0gADAzMDEG\n"
    "CCsGAQUFBwIBFiVodHRwczovL2NlcnRzLmdvZGFkZHkuY29tL3JlcG9zaXRvcnkv\n"
    "MA0GCSqGSIb3DQEBCwUAA4IBAQAIfmyTEMg4uJapkEv/oV9PBO9sPpyIBslQj6Zz\n"
    "91cxG7685C/b+LrTW+C05+Z5Yg4MotdqY3MxtfWoSKQ7CC2iXZDXtHwlTxFWMMS2\n"
    "RJ17LJ3lXubvDGGqv+QqG+6EnriDfcFDzkSnE3ANkR/0yBOtg2DZ2HKocyQetawi\n"
    "DsoXiWJYRBuriSUBAA/NxBti21G00w9RKpv0vHP8ds42pM3Z2Czqrpv1KrKQ0U11\n"
    "GIo/ikGQI31bS/6kA1ibRrLDYGCD+H1QQc7CoZDDu+8CL9IVVO5EFdkKrqeKM+2x\n"
    "LXY2JtwE65/3YR8V3Idv7kaWKK2hJn0KCacuBKONvPi8BDAB\n"
    "-----END CERTIFICATE-----\n";
void mqtt_app_start(void)
{
    esp_log_level_set("mqtt_client", ESP_LOG_WARN);
    if (wifiConnected == 0)
    {
        ESP_LOGI(TAG, "WIFI not connected: MQTT ERROR");
        return;
    }

    if (mqttClient != NULL)
    {
        ESP_LOGI(TAG, "MQTT already started");
        esp_mqtt_client_reconnect(mqttClient);
        return;
    }
    char uri[200];
    esp_mqtt_client_config_t mqtt_cfg = {};

    if (config.values.mode == MODE_TUYA)
    {
        // TUYA MQTT
        char client_id[100];
        sprintf(client_id, "tuyalink_%s", config.values.tuya.deviceId);
        mqtt_cfg.credentials.client_id = client_id;
        sprintf(uri, "mqtts://%s:%d", TUYA_SERVERS[config.values.tuya.server], 8883);

        time_t now = getTimestamp();

        char username[150];

        char password[150];
        sprintf(username, "%s|signMethod=hmacSha256,timestamp=%llu,secureMode=1,accessType=1", config.values.tuya.deviceId, now);
        sprintf(password, "deviceId=%s,timestamp=%llu,secureMode=1,accessType=1", config.values.tuya.deviceId, now);

        unsigned char hash[32];
        mbedtls_md_context_t ctx;
        mbedtls_md_type_t md_type = MBEDTLS_MD_SHA256;
        mbedtls_md_init(&ctx);
        mbedtls_md_setup(&ctx, mbedtls_md_info_from_type(md_type), 1);
        mbedtls_md_hmac_starts(&ctx, (const unsigned char *)config.values.tuya.deviceSecret, strlen(config.values.tuya.deviceSecret));
        mbedtls_md_hmac_update(&ctx, (const unsigned char *)password, strlen(password));
        mbedtls_md_hmac_finish(&ctx, hash);
        mbedtls_md_free(&ctx);

        char strHash[65];
        for (int i = 0; i < sizeof(hash); i++)
        {
            sprintf(strHash + i * 2, "%02x", hash[i]);
        }
        strHash[64] = '\0';

        mqtt_cfg.credentials.username = username;
        mqtt_cfg.credentials.authentication.password = strHash;
        mqtt_cfg.broker.verification.certificate = (const char *)tuyaMqttCert;

        ESP_LOGI(TAG, "TUYA MQTT: %s", uri);
        ESP_LOGI(TAG, "Username: %s", username);
        ESP_LOGI(TAG, "pw: %s", password);
        ESP_LOGI(TAG, "pw: %s", strHash);
        ESP_LOGI(TAG, "client_id: %s", client_id);
    }
    else
    {
        // HOME ASSISTANT / MQTT
        sprintf(uri, "mqtt://%s:%d", config.values.mqtt.host, config.values.mqtt.port);
        mqtt_cfg.credentials.username = config.values.mqtt.username;
        mqtt_cfg.credentials.authentication.password = config.values.mqtt.password;
    }

    mqtt_cfg.broker.address.uri = uri;

    mqttClient = esp_mqtt_client_init(&mqtt_cfg);
    /* The last argument may be used to pass data to the event handler, in this example mqtt_event_handler */
    esp_mqtt_client_register_event(mqttClient, (esp_mqtt_event_id_t)ESP_EVENT_ANY_ID, mqtt_event_handler, NULL);
    esp_mqtt_client_start(mqttClient);

    if (config.values.mode != MODE_TUYA)
    {
        // HOME ASSISTANT / MQTT
        for (int i = 0; i < sizeof(sensors) / sizeof(sensors[0]); i++)
        {
            if (strcmp(sensors[i].type, TYPE_NUMBER) == 0)
            {
                char topic[100];
                sprintf(topic, MQTT_ID "/%s", sensors[i].unique_id);
                esp_mqtt_client_subscribe(mqttClient, topic, 1);
            }
        }
    }
}

void createSensor(char *json, char *config_topic, sensorConfig sensor)
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
    sensorConfig["unique_id"] = sensor.unique_id;
    sensorConfig["object_id"] = sensor.unique_id;

    char state_topic[100];
    sprintf(state_topic, "~/%s", sensor.unique_id);
    sprintf(config_topic, "homeassistant/%s/%s/%s/config", sensor.type, MQTT_ID, sensor.unique_id);

    if (strcmp(sensor.type, TYPE_NUMBER) == 0)
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
    if (strlen(sensor.device_class) > 0)
    {
        sensorConfig["device_class"] = sensor.device_class;
    }
    if (strlen(sensor.unit_of_measurement) > 0)
    {
        sensorConfig["unit_of_measurement"] = sensor.unit_of_measurement;
    }
    if (strlen(sensor.value_template) > 0)
    {
        sensorConfig["value_template"] = sensor.value_template;
    }
    if (strlen(sensor.icon) > 0)
    {
        sensorConfig["icon"] = sensor.icon;
    }
    if (sensor.realTime)
    {
        sensorConfig["expire_after"] = config.values.refreshRate * 2;
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

    for (int i = 0; i < sizeof(sensors) / sizeof(sensors[0]); i++)
    {
        createSensor(mqttBuffer, config_topic, sensors[i]);
        esp_mqtt_client_publish(mqttClient, config_topic, mqttBuffer, 0, 2, 1);
    }
    ESP_LOGI(TAG, "Home Assistant Discovery done");
}

void sendHAMqtt(LinkyData *linky)
{
    static uint8_t HAConfigured = 0;
    vTaskDelay(500 / portTICK_PERIOD_MS);
    if (config.values.mode == MODE_MQTT_HA && HAConfigured == 0)
    {
        setupHomeAssistantDiscovery();
        HAConfigured = 1;
    }

    char topic[150];
    char value[20];

    mqttSendTimout = MILLIS + 10000;
    mqttSendCount = 0;
    linky->timestamp = getTimestamp();
    const void *values[] = {&linky->ADCO, &linky->OPTARIF, &linky->ISOUSC, &linky->BASE, &linky->HCHC, &linky->HCHP, &linky->PTEC, &linky->IINST, &linky->IMAX, &linky->PAPP, &linky->HHPHC, &linky->MOTDETAT, &linky->timestamp, &config.values.refreshRate};

    const uint8_t sensorsCount = sizeof(values) / sizeof(values[0]);
    for (int i = 0; i < sensorsCount; i++)
    {
        if (strcmp(sensors[i].type, TYPE_NUMBER) == 0)
        {
            continue;
        }
        if (sensors[i].valueType == TYPE_UINT32)
        {
            sprintf(topic, "%s/%s", config.values.mqtt.topic, (char *)sensors[i].unique_id);
            sprintf(value, "%lu", *(uint32_t *)(values[i]));
        }
        else if (sensors[i].valueType == TYPE_UINT16)
        {
            sprintf(topic, "%s/%s", config.values.mqtt.topic, (char *)sensors[i].unique_id);
            sprintf(value, "%u", *(int16_t *)(values[i]));
        }
        else
        {
            sprintf(topic, "%s/%s", config.values.mqtt.topic, (char *)sensors[i].unique_id);
            sprintf(value, "%s", (char *)(values[i]));
        }
        esp_mqtt_client_publish(mqttClient, topic, value, 0, 2, 0);
    }

    // vTaskDelay(5000 / portTICK_PERIOD_MS);
    while ((mqttSendTimout > MILLIS) && mqttSendCount < sensorsCount)
    {
        vTaskDelay(100 / portTICK_PERIOD_MS);
        // ESP_LOGI(TAG, "MQTT send %d/%d", mqttSendCount, sensorsCount);
    }
    if (mqttSendCount < sensorsCount)
    {
        ESP_LOGE(TAG, "Send Timeout");
    }
    else
    {
        ESP_LOGI(TAG, "Send Done");
    }
}

void sendTuyaMqtt(LinkyData *linky)
{
    time_t now = getTimestamp();
    DynamicJsonDocument json(1024);

    json["msgId"] = "1";
    json["time"] = now;
    json["sys"]["ack"] = 1;
    json["data"]["HCHP"]["value"] = linky->BASE;
    json["data"]["HCHP"]["time"] = now;
    json["data"]["HCHC"]["value"] = linky->HCHC;
    json["data"]["HCHC"]["time"] = now;

    char mqttBuffer[1024];
    serializeJson(json, mqttBuffer, 1024);

    char topic[100];
    sprintf(topic, "tylink/%s/thing/property/report", config.values.tuya.deviceId);
    esp_mqtt_client_publish(mqttClient, topic, mqttBuffer, 0, 1, 0);
    vTaskDelay(5000 / portTICK_PERIOD_MS);
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
        sendHAMqtt(linky);
        break;
    case MODE_TUYA:
        sendTuyaMqtt(linky);
        break;
    default:
        return;
    }
    mqtt_stop();
    mqttConnected = false;
}

void mqtt_stop()
{
    if (mqttClient != NULL)
    {
        esp_mqtt_client_disconnect(mqttClient);
        esp_mqtt_client_unregister_event(mqttClient, (esp_mqtt_event_id_t)ESP_EVENT_ANY_ID, mqtt_event_handler);
        // esp_mqtt_client_stop(mqttClient);
        esp_mqtt_client_destroy(mqttClient);
        mqttClient = NULL;
        vTaskDelay(1000 / portTICK_PERIOD_MS);
    }
}