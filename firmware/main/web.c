/**
 * @file web.cpp
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
#include "web.h"
#include "config.h"
#include "gpio.h"
#include "wifi.h"
#include "common.h"
#include "led.h"

/*==============================================================================
 Local Define
===============================================================================*/
#define TAG "WEB"

/*==============================================================================
 Local Macro
===============================================================================*/

/*==============================================================================
 Local Type
===============================================================================*/

/*==============================================================================
 Local Function Declaration
===============================================================================*/
static void web_create_http_url(char *url, const char *host, const char *path);
static esp_err_t web_http_send_data_handler(esp_http_client_event_handle_t evt);

/*==============================================================================
Public Variable
===============================================================================*/

/*==============================================================================
 Local Variable
===============================================================================*/

/*==============================================================================
Function Implementation
===============================================================================*/

void web_preapare_json_data(linky_data_t *data, char count, char **json)
{
    cJSON *jsonObject = cJSON_CreateObject(); // Create the root object
    cJSON_AddStringToObject(jsonObject, "TOKEN", config_values.web.token);
    cJSON_AddNumberToObject(jsonObject, "VCONDO", gpio_get_vcondo());
    cJSON *dataObject = cJSON_CreateArray(); // Create the data array
    for (int i = 0; i < count; i++)          // Add data to the array
    {
        ESP_LOGI(TAG, "Data index: %d: timestamp: %lld", i, data[i].timestamp);
        cJSON *dataItem = cJSON_CreateObject();
        for (uint32_t j = 0; j < LinkyLabelListSize; j++)
        {
            if (LinkyLabelList[j].data == NULL)
            {
                continue;
            }
            if (linky_mode != LinkyLabelList[j].mode && LinkyLabelList[j].mode != ANY)
            {
                continue;
            }
            uint8_t found = 0;
            for (uint32_t k = 0; k < linky_protected_data_size; k++)
            {
                if (LinkyLabelList[j].data == linky_protected_data[k])
                {
                    found = 1;
                    continue;
                }
            }
            if (found)
            {
                continue;
            }
            uint32_t delta_in_data = (char *)LinkyLabelList[j].data - (char *)&linky_data;
            ESP_LOGD(TAG, "Adress in data: 0x%lx", delta_in_data);
            void *value = (char *)&data[i] + delta_in_data;
            ESP_LOGD(TAG, "Adress in value: 0x%p", value);
            switch (LinkyLabelList[j].type)
            {
            case UINT8:
                if (*(uint8_t *)value == UINT8_MAX)
                {
                    continue;
                }
                ESP_LOGD(TAG, "Name: %s Type: UINT8 Value: %d", LinkyLabelList[j].label, *(uint8_t *)value);
                cJSON_AddNumberToObject(dataItem, LinkyLabelList[j].label, *(uint8_t *)value);
                break;
            case UINT16:
                if (*(uint16_t *)value == UINT16_MAX)
                {
                    continue;
                }
                ESP_LOGD(TAG, "Name: %s Type: UINT16 Value: %d", LinkyLabelList[j].label, *(uint16_t *)value);
                cJSON_AddNumberToObject(dataItem, LinkyLabelList[j].label, *(uint16_t *)value);
                break;
            case UINT32:
                if (*(uint32_t *)value == UINT32_MAX)
                {
                    continue;
                }
                if (LinkyLabelList[j].device_class == ENERGY && *(uint32_t *)value == 0)
                {
                    continue;
                }
                ESP_LOGD(TAG, "Name: %s Type: UINT32 Value: %ld", LinkyLabelList[j].label, *(uint32_t *)value);
                cJSON_AddNumberToObject(dataItem, LinkyLabelList[j].label, *(uint32_t *)value);
                break;
            case UINT64:
                if (*(uint64_t *)value == UINT64_MAX)
                {
                    continue;
                }
                if (LinkyLabelList[j].device_class == ENERGY && *(uint64_t *)value == 0)
                {
                    continue;
                }
                ESP_LOGD(TAG, "Name: %s Type: UINT64 Value: %lld", LinkyLabelList[j].label, *(uint64_t *)value);
                cJSON_AddNumberToObject(dataItem, LinkyLabelList[j].label, *(uint64_t *)value);
                break;
            case STRING:
                if (strlen((char *)value) == 0)
                {
                    continue;
                }
                ESP_LOGD(TAG, "Name: %s Type: STRING Value: %s", LinkyLabelList[j].label, (char *)value);
                cJSON_AddStringToObject(dataItem, LinkyLabelList[j].label, (char *)value);
                break;
            case UINT32_TIME:
                if (*(uint32_t *)value == UINT32_MAX)
                {
                    continue;
                }
                ESP_LOGD(TAG, "Name: %s Type: UINT32_TIME Value: %ld", LinkyLabelList[j].label, *(uint32_t *)value);
                cJSON_AddNumberToObject(dataItem, LinkyLabelList[j].label, *(uint32_t *)value);
                break;
            case BOOL:
                ESP_LOGD(TAG, "Name: %s Type: BOOL Value: %d", LinkyLabelList[j].label, *(bool *)value);
                cJSON_AddBoolToObject(dataItem, LinkyLabelList[j].label, *(bool *)value);
                break;
            default:
                break;
            }
        }
        cJSON_AddItemToArray(dataObject, dataItem);
    }
    if (count == 0)
    {
        // Send empty data to server to keep the connection alive
        cJSON_AddStringToObject(jsonObject, "ERROR", "Cant read data from linky");
    }
    cJSON_AddItemToObject(jsonObject, "data", dataObject); // Add the data array to the root object
    *json = cJSON_PrintUnformatted(jsonObject);            // Convert the json object to string
    cJSON_Delete(jsonObject);                              // Delete the json object
}

static esp_err_t web_http_send_data_handler(esp_http_client_event_handle_t evt)
{
    switch (evt->event_id)
    {
    case HTTP_EVENT_ON_DATA:
        printf("Config: %.*s", evt->data_len, (char *)evt->data);
        break;

    default:
        break;
    }
    return ESP_OK;
}

uint8_t wifi_send_to_server(const char *json)
{
    if (strlen(config_values.web.host) == 0 || strlen(config_values.web.postUrl) == 0)
    {
        ESP_LOGE(TAG, "host or postUrl not set");
        return 0;
    }
    led_start_pattern(LED_SENDING);

    char url[100] = {0};
    web_create_http_url(url, config_values.web.host, config_values.web.postUrl);
    // setup post request
    esp_http_client_config_t config_post;
    memset(&config_post, 0, sizeof(config_post));
    config_post.url = url;
    config_post.cert_pem = NULL;
    config_post.method = HTTP_METHOD_POST;
    config_post.event_handler = web_http_send_data_handler;

    // setup client
    esp_http_client_handle_t client = esp_http_client_init(&config_post);
    esp_http_client_set_post_field(client, json, strlen(json));
    esp_http_client_set_header(client, "Content-Type", "application/json");

    // send post request
    esp_http_client_perform(client);
    esp_http_client_cleanup(client);

    led_stop_pattern(LED_SENDING);
    led_start_pattern(LED_SEND_OK);
    return 1;
}

esp_err_t wifi_http_get_config_handler(esp_http_client_event_handle_t evt)
{
    switch (evt->event_id)
    {
    case HTTP_EVENT_ON_DATA:
        ESP_LOGI(TAG, "Config: %.*s", evt->data_len, (char *)evt->data);
        cJSON *json = cJSON_Parse(evt->data);
        cJSON *refresh_rate = cJSON_GetObjectItem(json, "refresh_rate");
        if (refresh_rate != NULL)
        {
            uint16_t value = refresh_rate->valueint;
            if (value < 10)
            {
                value = 10;
            }
            if (value > 3600)
            {
                value = 3600;
            }
            config_values.refresh_rate = value;
            ESP_LOGI(TAG, "Set refresh_rate: %d", value);
        }
        cJSON *store_before_send = cJSON_GetObjectItem(json, "store_before_send");
        if (store_before_send != NULL)
        {
            uint8_t value = store_before_send->valueint;
            if (store_before_send->valueint < 0)
            {
                value = 0;
            }
            else if (store_before_send->valueint > MAX_DATA_INDEX)
            {
                value = MAX_DATA_INDEX;
            }
            config_values.web.store_before_send = value;
            ESP_LOGI(TAG, "Set store_before_send: %d", value);
        }
        break;

    default:
        break;
    }
    return ESP_OK;
}

/**
 * @brief Get config from server and save it in EEPROM
 *
 */
void wifi_http_get_config_from_server()
{
    ESP_LOGI(TAG, "get config from server");
    char url[100] = {0};
    web_create_http_url(url, config_values.web.host, config_values.web.configUrl);
    strcat(url, "?token=");
    strcat(url, config_values.web.token);
    ESP_LOGI(TAG, "url: %s", url);

    esp_http_client_config_t config_get;
    memset(&config_get, 0, sizeof(config_get));
    config_get.url = url;
    config_get.cert_pem = NULL;
    config_get.method = HTTP_METHOD_GET;
    config_get.event_handler = wifi_http_get_config_handler;

    esp_http_client_handle_t client = esp_http_client_init(&config_get);
    esp_http_client_perform(client);
    esp_http_client_cleanup(client);
}

/**
 * @brief Create a Http Url (http://host/path)
 *
 * @param url the destination url
 * @param host the host
 * @param path the path
 */
static void web_create_http_url(char *url, const char *host, const char *path)
{
    url = strcat(url, "http://");
    url = strcat(url, host);
    url = strcat(url, path);
}