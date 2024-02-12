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

void web_preapare_json_data(linky_data_t *data, char dataIndex, char *json, unsigned int jsonSize)
{
    cJSON *jsonObject = cJSON_CreateObject(); // Create the root object
    cJSON_AddStringToObject(jsonObject, "TOKEN", config_values.web.token);
    cJSON_AddNumberToObject(jsonObject, "VCONDO", gpio_get_vcondo());
    cJSON *dataObject = cJSON_CreateArray(); // Create the data array
    for (int i = 0; i < dataIndex; i++)      // Add data to the array
    {
        cJSON *dataItem = cJSON_CreateObject();
        switch (linky_mode)
        {
        case MODE_HIST:
            cJSON_AddNumberToObject(dataItem, "DATE", data[i].timestamp);
            cJSON_AddStringToObject(dataItem, "ADCO", data[i].hist.ADCO);
            cJSON_AddStringToObject(dataItem, "OPTARIF", data[i].hist.OPTARIF);
            cJSON_AddNumberToObject(dataItem, "ISOUSC", data[i].hist.ISOUSC);
            if (data[i].hist.BASE != 0)
                cJSON_AddNumberToObject(dataItem, "BASE", data[i].hist.BASE);
            if (data[i].hist.HCHC != 0)
                cJSON_AddNumberToObject(dataItem, "HCHC", data[i].hist.HCHC);
            if (data[i].hist.HCHP != 0)
                cJSON_AddNumberToObject(dataItem, "HCHP", data[i].hist.HCHP);
            cJSON_AddStringToObject(dataItem, "PTEC", data[i].hist.PTEC);
            cJSON_AddNumberToObject(dataItem, "IINST", data[i].hist.IINST);
            cJSON_AddNumberToObject(dataItem, "IMAX", data[i].hist.IMAX);
            cJSON_AddNumberToObject(dataItem, "PAPP", data[i].hist.PAPP);
            cJSON_AddStringToObject(dataItem, "HHPHC", data[i].hist.HHPHC);
            cJSON_AddStringToObject(dataItem, "MOTDETAT", data[i].hist.MOTDETAT);
            break;
        case MODE_STD:
            ESP_LOGI("WEB", "MODE_STD not implemented yet");
            break;
        default:
            break;
        }
        cJSON_AddItemToArray(dataObject, dataItem);
    }

    if (dataIndex == 0)
    {
        // Send empty data to server to keep the connection alive
        cJSON_AddStringToObject(jsonObject, "ERROR", "Cant read data from linky");
        cJSON *dataItem = cJSON_CreateObject();
        cJSON_AddNumberToObject(dataItem, "DATE", wifi_get_timestamp());
        cJSON_AddNullToObject(dataItem, "BASE");
        cJSON_AddNullToObject(dataItem, "HCHC");
        cJSON_AddNullToObject(dataItem, "HCHP");
        cJSON_AddItemToArray(dataObject, dataItem);
    }
    cJSON_AddItemToObject(jsonObject, "data", dataObject); // Add the data array to the root object
    char *jsonString = cJSON_PrintUnformatted(jsonObject); // Convert the json object to string
    strncpy(json, jsonString, jsonSize);                   // Copy the string to the buffer
    free(jsonString);                                      // Free the memory
    cJSON_Delete(jsonObject);                              // Delete the json object
}

static esp_err_t web_http_send_data_handler(esp_http_client_event_handle_t evt)
{
    switch (evt->event_id)
    {
    case HTTP_EVENT_ON_DATA:
        printf("Config: %.*s\n", evt->data_len, (char *)evt->data);
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
        printf("Config: %.*s\n", evt->data_len, (char *)evt->data);
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