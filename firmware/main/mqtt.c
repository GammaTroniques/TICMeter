
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
// #define LOG_LOCAL_LEVEL ESP_LOG_VERBOSE
#include "esp_log.h"
#include "mqtt.h"
#include "wifi.h"
#include "gpio.h"
#include "led.h"
#include "cJSON.h"
#include "esp_ota_ops.h"
#include "mbedtls/md.h"

/*==============================================================================
 Local Define
===============================================================================*/
#define TAG "MQTT"
#define STATIC_VALUE 0
#define REAL_TIME 1

#define MQTT_ID "TICMeter"      // for the home assistant discovery
#define MQTT_SEND_TIMEOUT 10000 // in ms
#define MANUFACTURER "GammaTroniques"
#define MQTT_QOS 1
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

typedef struct
{
    char name[20];
    char unique_id_base[20];
    char ha_identifier_topic[50]; // ADCO or ADSC home assistant topic
    char ha_discovery_configured;
    char ha_discovery_configured_temp;
} mqtt_topic_t;

// #define MQTT_DEBUG

#ifdef MQTT_DEBUG
typedef struct
{
    int id;
    char name[150];
    char value[100];
} mqtt_debug_t;

#endif
/*==============================================================================
 Local Function Declaration
===============================================================================*/
static void log_error_if_nonzero(const char *message, int error_code);
static void mqtt_create_sensor(char *json, char *config_topic, LinkyGroup sensor);
void mqtt_setup_ha_discovery();
void mqtt_topic_comliance(char *topic, int size);
void mqtt_disconnect_task(void *pvParameters);

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

static mqtt_topic_t mqtt_topics = {0};
static EventGroupHandle_t mqtt_event_group = NULL;
static esp_mqtt_connect_return_code_t last_return_code = 0;
static esp_mqtt_error_type_t last_error_type;

#ifdef MQTT_DEBUG
static mqtt_debug_t mqtt_messages[100];
static int mqtt_messages_count = 0;
#endif
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

static void mqtt_remove_plus(char *topic)
{
    for (int j = 0; j < strlen(topic); j++)
    {
        if (topic[j] == '+')
        {
            topic[j] = '_';
        }
    }
}

static void mqtt_create_sensor(char *json, char *config_topic, LinkyGroup sensor)
{
    const esp_app_desc_t *app_desc = esp_app_get_description();
    cJSON *jsonDevice = cJSON_CreateObject(); // Create the root object
    cJSON_AddStringToObject(jsonDevice, "name", mqtt_topics.name);
    cJSON_AddStringToObject(jsonDevice, "mdl", app_desc->project_name);
    cJSON_AddStringToObject(jsonDevice, "mf", MANUFACTURER);
    cJSON_AddStringToObject(jsonDevice, "sw", app_desc->version);
    cJSON_AddStringToObject(jsonDevice, "sn", efuse_values.serial_number);
    char hw_version[15];
    snprintf(hw_version, sizeof(hw_version), "%d.%d.%d", efuse_values.hw_version[0], efuse_values.hw_version[1], efuse_values.hw_version[2]);
    cJSON_AddStringToObject(jsonDevice, "hw", hw_version);
    cJSON_AddStringToObject(jsonDevice, "ids", efuse_values.serial_number);

    cJSON *sensorConfig = cJSON_CreateObject(); // Create the root object
    cJSON_AddStringToObject(sensorConfig, "~", config_values.mqtt.topic);
    cJSON_AddStringToObject(sensorConfig, "name", sensor.name);
    char uniq_id[50];
    snprintf(uniq_id, sizeof(uniq_id), "%s_%s", mqtt_topics.unique_id_base, sensor.label);
    cJSON_AddStringToObject(sensorConfig, "uniq_id", uniq_id);
    cJSON_AddStringToObject(sensorConfig, "obj_id", uniq_id);

    char state_topic[100];
    snprintf(state_topic, sizeof(state_topic), "~/%s", sensor.label);
    linky_label_type_t type = sensor.type;
    if (sensor.device_class == CLASS_BOOL)
    {
        type = BOOL;
    }
    snprintf(config_topic, sizeof(state_topic), "homeassistant/%s/%s/%s/config", ha_sensors_str[type], mqtt_topics.name, sensor.label);
    if (strcmp(sensor.label, "ADCO") == 0 || strcmp(sensor.label, "ADSC") == 0)
    {
        strncpy(mqtt_topics.ha_identifier_topic, config_topic, sizeof(mqtt_topics.ha_identifier_topic));
    }

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
        mqtt_remove_plus(state_topic);
        cJSON_AddStringToObject(sensorConfig, "stat_t", state_topic);
    }
    if (sensor.device_class == TIMESTAMP)
    {
        cJSON_AddStringToObject(sensorConfig, "val_tpl", "{{ as_datetime(value) }}");
    }

    if (HADeviceClassStr[sensor.device_class] && strlen(HADeviceClassStr[sensor.device_class]) > 0)
    {
        cJSON_AddStringToObject(sensorConfig, "dev_cla", HADeviceClassStr[sensor.device_class]);
    }
    if (strlen(sensor.icon) > 0)
    {
        cJSON_AddStringToObject(sensorConfig, "icon", sensor.icon);
    }

    if (sensor.device_class != NONE_CLASS && sensor.device_class != TIMESTAMP && sensor.device_class != CLASS_BOOL)
    {
        cJSON_AddStringToObject(sensorConfig, "unit_of_meas", HAUnitsStr[sensor.device_class]);
    }

    if (sensor.realTime == REAL_TIME)
    {
        cJSON_AddNumberToObject(sensorConfig, "exp_aft", config_values.refresh_rate * 4);
    }

    switch (sensor.device_class)
    {
    case ENERGY:
    case ENERGY_Q:
        cJSON_AddStringToObject(sensorConfig, "stat_cla", "total_increasing");
        break;

    case POWER_kVA:
    case POWER_VA:
    case POWER_Q:
    case POWER_W:
    case POWER_kW:
    case CURRENT:
    case TENSION:
        cJSON_AddStringToObject(sensorConfig, "stat_cla", "measurement");
        break;
    default:
        break;
    }

    cJSON_AddItemToObject(sensorConfig, "dev", jsonDevice);
    char *jsonString = cJSON_PrintUnformatted(sensorConfig);
    strncpy(json, jsonString, 1024);
    free(jsonString);
    cJSON_Delete(sensorConfig);
}

uint8_t mqtt_prepare_publish(linky_data_t *linkydata)
{
    static bool first = true;
    mqtt_sensors_count = 0;
    mqtt_sent_count = 0;
    if (mqtt_state == MQTT_DEINIT)
    {
        ESP_LOGE(TAG, "Cant prepare data: MQTT not initialized");
        return 0;
    }

    uint8_t has_error = 0;
    mqtt_topics.ha_discovery_configured_temp = 0;
    ESP_LOGI(TAG, "ha_discovery_configured = %d", mqtt_topics.ha_discovery_configured);
    if (config_values.mode == MODE_MQTT_HA && (/*&& mqtt_topics.ha_discovery_configured == 0 || */ first))
    {
        first = false;
        ESP_LOGW(TAG, "Home Assistant Discovery not configured, configuring...");
        ESP_LOGI(TAG, "Reread linky to be sure to have all data");
        linky_update(false);
        linky_update(false);
        linky_update(false);
        mqtt_setup_ha_discovery(linkydata);
    }

    char topic[150];
    char strValue[100];

    ESP_LOGI(TAG, "Pre-send Outbox size: %d", esp_mqtt_client_get_outbox_size(mqtt_client));

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
            if (LinkyLabelList[i].device_class == ENERGY && *(uint32_t *)(LinkyLabelList[i].data) == 0)
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
            time_label_t *timeLabel = (time_label_t *)LinkyLabelList[i].data;
            if (timeLabel->value == UINT32_MAX)
                continue;
            snprintf(strValue, sizeof(strValue), "%lu", timeLabel->value);
            break;
        }
        case HA_NUMBER:
            continue;
            break;

        default:
            break;
        }

        if (LinkyLabelList[i].data == &linky_mode)
        {
            switch (linky_mode)
            {
            case MODE_HIST:
                snprintf(strValue, sizeof(strValue), "Historique");
                break;
            case MODE_STD:
                snprintf(strValue, sizeof(strValue), "Standard");
                break;
            default:
                snprintf(strValue, sizeof(strValue), "Inconnu");
                break;
            }
        }
        else if (LinkyLabelList[i].data == &linky_three_phase)
        {
            if (linky_three_phase == 1)
            {
                snprintf(strValue, sizeof(strValue), "Triphasé");
            }
            else
            {
                snprintf(strValue, sizeof(strValue), "Monophasé");
            }
        }
        else if (LinkyLabelList[i].device_class == TIME_M)
        {
            snprintf(strValue, sizeof(strValue), "%lu", *((uint32_t *)LinkyLabelList[i].data) / 1000);
        }

        mqtt_sensors_count++;
        // esp_mqtt_client_publish(mqtt_client, topic, strValue, 0, 2, 0);
        mqtt_topic_comliance(topic, sizeof(topic));
        int ret = esp_mqtt_client_enqueue(mqtt_client, topic, strValue, 0, MQTT_QOS, 0, true);
        ESP_LOGD(TAG, "Prepared id= %d %s = %s", ret, topic, strValue);

#ifdef MQTT_DEBUG
        mqtt_messages[mqtt_messages_count++].id = ret;
        strncpy(mqtt_messages[mqtt_messages_count - 1].name, topic, sizeof(topic));
        strncpy(mqtt_messages[mqtt_messages_count - 1].value, strValue, sizeof(strValue));
#endif
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
    bool delete = false;

    for (int i = 0; i < LinkyLabelListSize; i++)
    {
        if (LinkyLabelList[i].data == NULL)
        {
            continue;
        }

        if (LinkyLabelList[i].mode != linky_mode && LinkyLabelList[i].mode != ANY)
        {
            delete = true;
        }

        ESP_LOGD(TAG, "HA Discovery: %s", LinkyLabelList[i].label);
        switch (LinkyLabelList[i].type)
        {
        case UINT8:
            if (*(uint8_t *)LinkyLabelList[i].data == UINT8_MAX)
            {
                delete = true;
            }
            ESP_LOGD(TAG, "Adding %s: value = %d", LinkyLabelList[i].label, *(uint8_t *)LinkyLabelList[i].data);
            break;
        case UINT16:
            if (*(uint16_t *)LinkyLabelList[i].data == UINT16_MAX)
            {
                delete = true;
            }
            ESP_LOGD(TAG, "Adding %s: value = %d", LinkyLabelList[i].label, *(uint16_t *)LinkyLabelList[i].data);
            break;
        case UINT32:
            if (*(uint32_t *)LinkyLabelList[i].data == UINT32_MAX)
            {
                delete = true;
            }
            if (LinkyLabelList[i].device_class == ENERGY && *(uint32_t *)(LinkyLabelList[i].data) == 0)
            {
                delete = true;
            }
            ESP_LOGD(TAG, "Adding %s: value = %ld", LinkyLabelList[i].label, *(uint32_t *)LinkyLabelList[i].data);
            break;
        case UINT64:
            if (*(uint64_t *)LinkyLabelList[i].data == UINT64_MAX)
            {
                delete = true;
            }
            ESP_LOGD(TAG, "Adding %s: value = %lld", LinkyLabelList[i].label, *(uint64_t *)LinkyLabelList[i].data);
            break;
        case STRING:
            if (strlen((char *)LinkyLabelList[i].data) == 0)
            {
                delete = true;
            }
            ESP_LOGD(TAG, "Adding %s: value = %s", LinkyLabelList[i].label, (char *)LinkyLabelList[i].data);
            break;
        case UINT32_TIME:
            if (((time_label_t *)LinkyLabelList[i].data)->value == UINT32_MAX)
            {
                delete = true;
            }
            ESP_LOGD(TAG, "Adding %s: value = %lu", LinkyLabelList[i].label, ((time_label_t *)LinkyLabelList[i].data)->value);
            break;
        case HA_NUMBER:
            break;
        default:
            ESP_LOGE(TAG, "Unknown type %d", LinkyLabelList[i].type);
            continue;
            break;
        }
        mqtt_create_sensor(mqttBuffer, config_topic, LinkyLabelList[i]);
        mqtt_topic_comliance(config_topic, sizeof(config_topic));
        if (delete)
        {
            ESP_LOGW(TAG, "Delete %s", config_topic);
            esp_mqtt_client_enqueue(mqtt_client, config_topic, "", 0, 1, 0, true);
            delete = false;
        }
        else
        {
            esp_mqtt_client_enqueue(mqtt_client, config_topic, mqttBuffer, 0, 2, 1, true);
        }

        mqtt_sensors_count++;
    }
    ESP_LOGI(TAG, "Home Assistant Discovery done");
}

void mqtt_event_handler(void *handler_args, esp_event_base_t base, int32_t event_id, void *event_data)
{
    ESP_LOGD(TAG, "Event dispatched from event loop base=%s, event_id=%" PRIi32 "", base, event_id);
    esp_mqtt_event_handle_t event = (esp_mqtt_event_handle_t)event_data;

    switch ((esp_mqtt_event_id_t)event_id)
    {
    case MQTT_EVENT_CONNECTED:
        if (mqtt_event_group)
        {
            xEventGroupSetBits(mqtt_event_group, BIT0);
        }
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
        if (config_values.mode == MODE_MQTT_HA && strlen(mqtt_topics.ha_identifier_topic) > 0)
        {
            ESP_LOGI(TAG, "Subscribing to identifier %s", mqtt_topics.ha_identifier_topic);
            esp_mqtt_client_subscribe(mqtt_client, mqtt_topics.ha_identifier_topic, 1);
        }

        break;
    case MQTT_EVENT_DISCONNECTED:
        if (mqtt_event_group)
        {
            xEventGroupSetBits(mqtt_event_group, BIT1);
        }
        if (mqtt_state != MQTT_FAILED)
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
#ifdef MQTT_DEBUG
        for (int i = 0; i < mqtt_messages_count; i++)
        {
            if (mqtt_messages[i].id == event->msg_id)
            {
                mqtt_messages[i].id = -1;
                break;
            }
        }
#endif
        mqtt_sent_count++;
        break;
    case MQTT_EVENT_DATA:
    {
        ESP_LOGI(TAG, "MQTT_EVENT_DATA: %.*s", event->topic_len, event->topic);
        char fullname[50];
        char strValue[512]; // prevent buffer overflow with long string (ha discovery)
        strncpy(fullname, event->topic, event->topic_len);
        fullname[event->topic_len] = '\0';
        strncpy(strValue, event->data, event->data_len);
        strValue[event->data_len] = '\0';

        if (strlen(fullname) == 0)
        {
            break;
        }
        if (config_values.mode == MODE_MQTT_HA)
        {
            if (strcmp(fullname, mqtt_topics.ha_identifier_topic) == 0)
            {
                ESP_LOGI(TAG, "Home Assistant discovery is already configured");
                mqtt_topics.ha_discovery_configured_temp = 1;
                mqtt_topics.ha_discovery_configured = 1;
                break;
            }
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
                    ((time_label_t *)LinkyLabelList[i].data)->value = atol(strValue);
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
        ESP_LOGI(TAG, "MQTT_EVENT_ERROR type=%d, code=%d, msgid=%d", event->error_handle->error_type, event->error_handle->connect_return_code, event->msg_id);

        switch (event->error_handle->error_type)
        {
        case MQTT_ERROR_TYPE_TCP_TRANSPORT:
            log_error_if_nonzero("reported from esp-tls", event->error_handle->esp_tls_last_esp_err);
            log_error_if_nonzero("reported from tls stack", event->error_handle->esp_tls_stack_err);
            log_error_if_nonzero("captured as transport's socket errno", event->error_handle->esp_transport_sock_errno);
            ESP_LOGI(TAG, "Last errno string (%s)", strerror(event->error_handle->esp_transport_sock_errno));
            break;

        default:
            break;
        }
        mqtt_state = MQTT_FAILED;
        last_error_type = event->error_handle->error_type;
        last_return_code = event->error_handle->connect_return_code;
        if (mqtt_event_group)
        {
            xEventGroupSetBits(mqtt_event_group, BIT2);
        }
        break;
    default:
        // ESP_LOGI(TAG, "Other event id:%d", event->event_id);
        break;
    }
}

int mqtt_init(void)
{
    esp_log_level_set("mqtt_client", ESP_LOG_WARN);

#ifdef MQTT_DEBUG
    ESP_LOGW(TAG, "MQTT_DEBUG enabled");
#endif

    if (wifi_state == WIFI_DISCONNECTED)
    {
        ESP_LOGI(TAG, "WIFI not connected: MQTT ERROR");
        return -1;
    }

    snprintf(mqtt_topics.name, sizeof(mqtt_topics.name), MQTT_ID "_%s", efuse_values.mac_address + 6);
    snprintf(mqtt_topics.unique_id_base, sizeof(mqtt_topics.unique_id_base), "TICMeter_%s_", efuse_values.mac_address + 6);
    mqtt_state = MQTT_CONNECTING;
    char uri[200];
    snprintf(uri, sizeof(uri), "mqtt://%s:%d", config_values.mqtt.host, config_values.mqtt.port);
    esp_mqtt_client_config_t mqtt_cfg = {
        // .session.message_retransmit_timeout = 500,
        .outbox.limit = 64 * 1024,
        .credentials.username = config_values.mqtt.username,
        .credentials.authentication.password = config_values.mqtt.password,
        .task = {
            .stack_size = 16 * 1024,
            .priority = 2,
        },
        .broker.address.uri = uri,
        .credentials.client_id = mqtt_topics.name,
    };
    mqtt_client = esp_mqtt_client_init(&mqtt_cfg);
    /* The last argument may be used to pass data to the event handler, in this example mqtt_event_handler */
    esp_mqtt_client_register_event(mqtt_client, (esp_mqtt_event_id_t)ESP_EVENT_ANY_ID, mqtt_event_handler, NULL);
    ESP_LOGI(TAG, "init done");
    return 1;
}

int mqtt_deinit()
{
    if (mqtt_client != NULL)
    {
        ESP_LOGI(TAG, "Deinit MQTT");
        esp_mqtt_client_destroy(mqtt_client);
        mqtt_client = NULL;
    }
    mqtt_state = MQTT_DEINIT;
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
    led_start_pattern(LED_SENDING);

#ifdef MQTT_DEBUG
    mqtt_messages_count = 0;
    for (int i = 0; i < mqtt_sensors_count; i++)
    {
        mqtt_messages[i].id = -1;
    }
#endif

    if (mqtt_state == MQTT_FAILED)
    {
        ESP_LOGI(TAG, "MQTT Failed state: reconnecting...");
        mqtt_state = MQTT_CONNECTING;
        err = esp_mqtt_client_reconnect(mqtt_client);
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
        if (mqtt_state == MQTT_DISCONNETED)
        {
            ESP_LOGE(TAG, "MQTT Exit: %d", mqtt_state);
            break;
        }
        if (mqtt_state == MQTT_FAILED)
        {
            ESP_LOGW(TAG, "MQTT faild");
            break;
            // esp_mqtt_client_disconnect(mqtt_client);
            // vTaskDelay(100 / portTICK_PERIOD_MS);
            // esp_mqtt_client_start(mqtt_client);
        }
        vTaskDelay(100 / portTICK_PERIOD_MS);
        ESP_LOGD(TAG, "Outbox size: %d", esp_mqtt_client_get_outbox_size(mqtt_client));
    }

    // ESP_LOGW(TAG, "set ha_discovery_configured to %d", mqtt_topics.ha_discovery_configured_temp);
    mqtt_topics.ha_discovery_configured = mqtt_topics.ha_discovery_configured_temp;
    led_stop_pattern(LED_SENDING);

#ifdef MQTT_DEBUG

    for (int i = 0; i < mqtt_messages_count; i++)
    {
        if (mqtt_messages[i].id != -1)
        {
            ESP_LOGE(TAG, "Message not sent: %d %s = %s", mqtt_messages[i].id, mqtt_messages[i].name, mqtt_messages[i].value);
        }
    }

#endif
    if (mqtt_state == MQTT_FAILED)
    {
        ESP_LOGE(TAG, "Send Failed: %d/%d", mqtt_sent_count, mqtt_sensors_count);
        goto error;
    }

    if (/*mqtt_sent_count < mqtt_sensors_count*/ esp_mqtt_client_get_outbox_size(mqtt_client) > 0)
    {
        ESP_LOGE(TAG, "Send Timeout: %d/%d", mqtt_sent_count, mqtt_sensors_count);
        goto error;
    }
    else
    {
        ESP_LOGI(TAG, "Send Done: %d msg", mqtt_sent_count);
    }
    xTaskCreate(mqtt_disconnect_task, "mqtt_disconnect_task", 4096, NULL, 5, NULL);
    led_start_pattern(LED_SEND_OK);
    return 1;
error:
    xTaskCreate(mqtt_disconnect_task, "mqtt_disconnect_task", 4096, NULL, 5, NULL);
    led_start_pattern(LED_SEND_FAILED);
    return 0;
}

void mqtt_disconnect_task(void *pvParameters)
{
    ESP_LOGI(TAG, "Disconnecting MQTT");
    mqtt_state = MQTT_DISCONNETED;
    esp_mqtt_client_disconnect(mqtt_client);
    esp_mqtt_client_stop(mqtt_client);
    vTaskDelete(NULL);
}

void mqtt_topic_comliance(char *topic, int size)
{
    for (int j = 0; j < size; j++)
    {
        if (topic[j] == '+')
        {
            topic[j] = '_';
        }
    }
}

esp_err_t mqtt_test(esp_mqtt_error_type_t *type, esp_mqtt_connect_return_code_t *return_code)
{
    esp_err_t err = ESP_OK;
    if (mqtt_event_group == NULL)
    {
        mqtt_event_group = xEventGroupCreate();
    }
    mqtt_deinit();
    mqtt_init();

    err = esp_mqtt_client_start(mqtt_client);
    if (err != ESP_OK)
    {
        ESP_LOGE(TAG, "Start failed with 0x%x", err);
        return err;
    }
    EventBits_t bits = xEventGroupWaitBits(mqtt_event_group,
                                           BIT0 | BIT1 | BIT2,
                                           pdFALSE,
                                           pdFALSE,
                                           30000 / portTICK_PERIOD_MS);

    xEventGroupClearBits(mqtt_event_group, BIT0 | BIT1 | BIT2);

    if (type != NULL)
    {
        *type = last_error_type;
    }
    if (return_code != NULL)
    {
        *return_code = last_return_code;
    }

    if (bits & BIT0)
    {
        return ESP_OK;
    }
    else
    {
        return ESP_FAIL;
    }
}