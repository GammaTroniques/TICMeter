/**
 * @file ota.c
 * @author Dorian Benech
 * @brief
 * @version 1.0
 * @date 2023-10-27
 *
 * @copyright Copyright (c) 2023 GammaTroniques
 *
 */

/*==============================================================================
 Local Include
===============================================================================*/
#include "ota.h"

#include "esp_http_client.h"
#include "esp_https_ota.h"
#include "esp_log.h"
#include "esp_ota_ops.h"
#include <esp_tls.h>

#include "lwip/err.h"
#include "lwip/sockets.h"
#include "lwip/sys.h"
#include "lwip/netdb.h"
#include "lwip/dns.h"
#include "esp_crt_bundle.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/event_groups.h"
#include "cJSON.h"
#include "wifi.h"
#include "gpio.h"
/*==============================================================================
 Local Define
===============================================================================*/

#define TAG "OTA"
#define OTA_DOMAIN "linky.gammatroniques.fr"
#define OTA_VERSION_URL "https://ticmeter.gammatroniques.fr/versions.json"
#define OTA_TIMEOUT_MS 10000
/*==============================================================================
 Local Macro
===============================================================================*/

/*==============================================================================
 Local Type
===============================================================================*/

/*==============================================================================
 Local Function Declaration
===============================================================================*/
static void ota_event_handler(void *arg, esp_event_base_t event_base, int32_t event_id, void *event_data);
static esp_err_t ota_validate_image_header(esp_app_desc_t *new_app_info);
static esp_err_t ota_http_client_init_cb(esp_http_client_handle_t http_client);
static esp_err_t ota_https_event_handler(esp_http_client_event_t *evt);
static uint8_t ota_version_compare(const char *current_version, const char *to_check_version);

/*==============================================================================
Public Variable
===============================================================================*/
extern const uint8_t root_ca_cert_pem_start[] asm("_binary_ca_cert_pem_start");
extern const uint8_t root_ca_cert_pem_end[] asm("_binary_ca_cert_pem_end");
ota_state_t ota_state = OTA_IDLE;

/*==============================================================================
 Local Variable
===============================================================================*/
char *ota_version_buffer = NULL;
uint32_t ota_version_buffer_size = 0;
/*==============================================================================
Function Implementation
===============================================================================*/

static int ota_json_parse_string(cJSON *json, const char *key, char *value, size_t value_size)
{
    assert(json != NULL);
    assert(key != NULL);
    assert(value != NULL);
    assert(value_size > 0);

    cJSON *json_value = cJSON_GetObjectItemCaseSensitive(json, key);
    if (json_value == NULL)
    {
        ESP_LOGE(TAG, "Parse Error: %s NULL", key);
        return -1;
    }
    if (!cJSON_IsString(json_value))
    {
        ESP_LOGE(TAG, "Parse Error: %s not a string", key);
        return -1;
    }
    if (strlen(json_value->valuestring) >= value_size)
    {
        ESP_LOGE(TAG, "Parse Error: %s too long", key);
        return -1;
    }
    int len = strlen(json_value->valuestring);
    int min = len < value_size ? len : value_size;
    strncpy(value, json_value->valuestring, min);
    return 0;
}

int ota_get_latest(ota_version_t *version)
{
    assert(version != NULL);
    memset(version, 0, sizeof(ota_version_t));
    const esp_app_desc_t *app_desc = esp_app_get_description();

    char user_agent[64];
    sprintf(user_agent, "TICMeter/%s %s", app_desc->version, "SERIAL");
    esp_http_client_config_t config = {
        .url = OTA_VERSION_URL,
        .cert_pem = (char *)root_ca_cert_pem_start,
        .method = HTTP_METHOD_GET,
        .event_handler = ota_https_event_handler,
        .user_agent = user_agent,
    };

    esp_http_client_handle_t client = esp_http_client_init(&config);
    esp_err_t err = esp_http_client_perform(client);

    if (err != ESP_OK)
    {
        ESP_LOGE(TAG, "HTTP Request Failed");
        return -1;
    }

    int status_code = esp_http_client_get_status_code(client);
    ESP_LOGI(TAG, "HTTP Status = %d, content_length = %lld",
             status_code,
             esp_http_client_get_content_length(client));

    esp_http_client_cleanup(client);

    if (status_code != 200)
    {
        ESP_LOGE(TAG, "HTTP Get: %s Status code: %d", OTA_VERSION_URL, status_code);
        return -1;
    }

    ESP_LOGD(TAG, "Version: %s", ota_version_buffer);
    cJSON *json = cJSON_Parse(ota_version_buffer);
    if (json == NULL)
    {
        const char *error_ptr = cJSON_GetErrorPtr();
        if (error_ptr != NULL)
        {
            ESP_LOGE(TAG, "Parse Error: %s", error_ptr);
        }
        else
        {
            ESP_LOGE(TAG, "Parse Error: %s", "Unknown");
        }
        return -1;
    }
    ota_json_parse_string(json, "latest", version->version, sizeof(version->version));
    //------------------------Parse Firmware------------------------
    cJSON *json_firmware = cJSON_GetObjectItemCaseSensitive(json, "firmware");
    if (json_firmware == NULL)
    {
        ESP_LOGE(TAG, "Parse Error: %s", "firmware not found");
        return -1;
    }
    if (!cJSON_IsArray(json_firmware))
    {
        ESP_LOGE(TAG, "Parse Error: %s", "firmware not array");
        return -1;
    }

    //------------------------Parse Firmware Item------------------------
    cJSON *json_firmware_item = NULL;

    cJSON_ArrayForEach(json_firmware_item, json_firmware)
    {
        ota_json_parse_string(json_firmware_item, "target", version->target, sizeof(version->target));
        if (strcmp(version->target, CONFIG_IDF_TARGET) != 0) // if target is not the same as the current target, continue
        {
            continue;
        }
        ota_json_parse_string(json_firmware_item, "hwVersion", version->hwVersion, sizeof(version->version));
        ota_json_parse_string(json_firmware_item, "url", version->url, sizeof(version->url));
        ota_json_parse_string(json_firmware_item, "md5", version->md5, sizeof(version->md5));
        break;
    }

    strncpy(version->currentVersion, app_desc->version, sizeof(version->currentVersion));
    free(ota_version_buffer);
    cJSON_Delete(json);
    ota_version_buffer = NULL;

    ESP_LOGI(TAG, "Current Version: %s", version->currentVersion);
    ESP_LOGI(TAG, "Latest version: %s", version->version);
    ESP_LOGI(TAG, "Target: %s", version->target);
    ESP_LOGI(TAG, "HW Version: %s", version->hwVersion);
    ESP_LOGI(TAG, "URL: %s", version->url);
    ESP_LOGI(TAG, "MD5: %s", version->md5);

    uint8_t result = ota_version_compare(version->currentVersion, version->version);
    if (result == 0)
    {
        ESP_LOGW(TAG, "No update available");
        return 0;
    }
    else if (result == 1)
    {
        ESP_LOGW(TAG, "Current version > latest version");
        return 0;
    }
    ESP_LOGI(TAG, "Update available");
    ota_state = OTA_AVAILABLE;
    if (!gpip_led_ota_task_handle)
    {
        xTaskCreate(gpio_led_task_ota, "gpio_led_task_update_available", 4 * 1024, NULL, 1, &gpip_led_ota_task_handle); // start update led task
    }

    return 1;
}

static void ota_event_handler(void *arg, esp_event_base_t event_base, int32_t event_id, void *event_data)
{
    if (event_base == ESP_HTTPS_OTA_EVENT)
    {
        switch (event_id)
        {
        case ESP_HTTPS_OTA_START:
            ESP_LOGI(TAG, "OTA started");
            break;
        case ESP_HTTPS_OTA_CONNECTED:
            ESP_LOGI(TAG, "Connected to server");
            break;
        case ESP_HTTPS_OTA_GET_IMG_DESC:
            ESP_LOGI(TAG, "Reading Image Description");
            break;
        case ESP_HTTPS_OTA_VERIFY_CHIP_ID:
            ESP_LOGI(TAG, "Verifying chip id of new image: %d", *(esp_chip_id_t *)event_data);
            break;
        case ESP_HTTPS_OTA_DECRYPT_CB:
            ESP_LOGI(TAG, "Callback to decrypt function");
            break;
        case ESP_HTTPS_OTA_WRITE_FLASH:
            ESP_LOGD(TAG, "Writing to flash: %d written", *(int *)event_data);
            break;
        case ESP_HTTPS_OTA_UPDATE_BOOT_PARTITION:
            ESP_LOGI(TAG, "Boot partition updated. Next Partition: %d", *(esp_partition_subtype_t *)event_data);
            break;
        case ESP_HTTPS_OTA_FINISH:
            ESP_LOGI(TAG, "OTA finish");
            break;
        case ESP_HTTPS_OTA_ABORT:
            ESP_LOGI(TAG, "OTA abort");
            break;
        }
    }
}

static esp_err_t ota_validate_image_header(esp_app_desc_t *new_app_info)
{
    if (new_app_info == NULL)
    {
        return ESP_ERR_INVALID_ARG;
    }

    const esp_partition_t *running = esp_ota_get_running_partition();
    esp_app_desc_t running_app_info;
    if (esp_ota_get_partition_description(running, &running_app_info) == ESP_OK)
    {
        ESP_LOGI(TAG, "Running firmware version: %s", running_app_info.version);
    }

    // if (memcmp(new_app_info->version, running_app_info.version, sizeof(new_app_info->version)) == 0)
    // {
    //     ESP_LOGW(TAG, "Current running version is the same as a new. We will not continue the update.");
    //     return ESP_FAIL;
    // }
    return ESP_OK;
}

static esp_err_t ota_http_client_init_cb(esp_http_client_handle_t http_client)
{
    esp_err_t err = ESP_OK;
    /* Uncomment to add custom headers to HTTP request */
    // err = esp_http_client_set_header(http_client, "Custom-Header", "Value");
    return err;
}

/**
 * @brief Get the version of the firmware
 *
 * @param current_version
 * @param to_check_version
 * @return uint8_t 0: same version, 1: current version is greater, 2: to_check_version is greater
 */
static uint8_t ota_version_compare(const char *current_version, const char *to_check_version)
{
    if (current_version == NULL || to_check_version == NULL)
    {
        return 0;
    }
    char *v1 = strdup(current_version);
    char *v2 = strdup(to_check_version);
    char *token1 = NULL;
    char *token2 = NULL;
    char *saveptr1 = NULL;
    char *saveptr2 = NULL;
    uint8_t result = 0;

    token1 = strtok_r(v1, ".", &saveptr1);
    token2 = strtok_r(v2, ".", &saveptr2);
    while (token1 != NULL && token2 != NULL)
    {
        if (atoi(token1) > atoi(token2))
        {
            result = 1;
            break;
        }
        else if (atoi(token1) < atoi(token2))
        {
            result = 2;
            break;
        }
        token1 = strtok_r(NULL, ".", &saveptr1);
        token2 = strtok_r(NULL, ".", &saveptr2);
    }
    if (token1 == NULL && token2 != NULL)
    {
        result = 2;
    }
    else if (token1 != NULL && token2 == NULL)
    {
        result = 1;
    }
    free(v1);
    free(v2);
    return result;
}

void ota_perform_task(void *pvParameter)
{
    if (!wifi_connect())
    {
        ESP_LOGE(TAG, "Wifi connect failed");
        goto ota_end;
    }

    ota_version_t version = {0};
    int ret = ota_get_latest(&version);
    ota_state = OTA_INSTALLING;
    if (ret == -1)
    {
        ESP_LOGE(TAG, "Get latest version failed");
        goto ota_end;
    }
    else if (ret == 0)
    {
        ESP_LOGI(TAG, "No update available");
        goto ota_end;
    }

    if (strcmp(version.target, CONFIG_IDF_TARGET) != 0)
    {
        ESP_LOGE(TAG, "Target not valid: %s != %s", version.target, CONFIG_IDF_TARGET);
        goto ota_end;
    }

    esp_err_t ota_finish_err = ESP_OK;
    ESP_LOGI(TAG, "Starting OTA...");
    esp_err_t err = esp_event_handler_register(ESP_HTTPS_OTA_EVENT, ESP_EVENT_ANY_ID, &ota_event_handler, NULL);
    if (err != ESP_OK)
    {
        ESP_LOGE(TAG, "esp_event_handler_register() failed with %s", esp_err_to_name(err));
        goto ota_end;
    }

    ESP_LOGI(TAG, "Starting download from %s", version.url);
    esp_http_client_config_t config = {
        .url = version.url,
        .cert_pem = (char *)root_ca_cert_pem_start,
        .timeout_ms = OTA_TIMEOUT_MS,
        .keep_alive_enable = true,
    };

    esp_https_ota_config_t ota_config = {
        .http_config = &config,
        .http_client_init_cb = ota_http_client_init_cb, // Register a callback to be invoked after esp_http_client is initialized
    };

    esp_https_ota_handle_t https_ota_handle = NULL;
    err = esp_https_ota_begin(&ota_config, &https_ota_handle);
    if (err != ESP_OK)
    {
        ESP_LOGE(TAG, "ESP HTTPS OTA Begin failed");
        goto ota_end;
    }

    esp_app_desc_t app_desc;
    err = esp_https_ota_get_img_desc(https_ota_handle, &app_desc);
    if (err != ESP_OK)
    {
        ESP_LOGE(TAG, "esp_https_ota_read_img_desc failed");
        goto ota_end;
    }
    err = ota_validate_image_header(&app_desc);
    if (err != ESP_OK)
    {
        ESP_LOGE(TAG, "image header verification failed");
        goto ota_end;
    }

    while (1)
    {
        err = esp_https_ota_perform(https_ota_handle);
        if (err != ESP_ERR_HTTPS_OTA_IN_PROGRESS)
        {
            break;
        }
        // esp_https_ota_perform returns after every read operation which gives user the ability to
        // monitor the status of OTA upgrade by calling esp_https_ota_get_image_len_read, which gives length of image
        // data read so far.
        ESP_LOGD(TAG, "Image bytes read: %d", esp_https_ota_get_image_len_read(https_ota_handle));
    }

    if (esp_https_ota_is_complete_data_received(https_ota_handle) != true)
    {
        // the OTA image was not completely received and user can customise the response to this situation.
        ESP_LOGE(TAG, "Complete data was not received.");
    }
    else
    {
        ota_finish_err = esp_https_ota_finish(https_ota_handle);
        if ((err == ESP_OK) && (ota_finish_err == ESP_OK))
        {
            ESP_LOGI(TAG, "ESP_HTTPS_OTA upgrade successful. Rebooting ...");
            ota_state = OTA_OK;
            vTaskDelay(3000 / portTICK_PERIOD_MS);
            esp_restart();
        }
        else
        {
            if (ota_finish_err == ESP_ERR_OTA_VALIDATE_FAILED)
            {
                ESP_LOGE(TAG, "Image validation failed, image is corrupted");
            }
            ESP_LOGE(TAG, "ESP_HTTPS_OTA upgrade failed 0x%x", ota_finish_err);
        }
    }
ota_end:
    ota_state = OTA_ERROR;
    esp_https_ota_abort(https_ota_handle);
    ESP_LOGE(TAG, "ESP_HTTPS_OTA upgrade failed");
    vTaskDelay(3000 / portTICK_PERIOD_MS);
    esp_restart();
}

static esp_err_t ota_https_event_handler(esp_http_client_event_t *evt)
{
    switch (evt->event_id)
    {
    case HTTP_EVENT_ON_CONNECTED:
        ESP_LOGI(TAG, "HTTP_EVENT_ON_CONNECTED");
        ota_version_buffer_size = 0;
        break;
    case HTTP_EVENT_ERROR:
        ESP_LOGE(TAG, "HTTP_EVENT_ERROR");
        break;
    case HTTP_EVENT_ON_DATA:
    {
        ESP_LOGI(TAG, "HTTP_EVENT_ON_DATA, len=%d", evt->data_len);
        if (!esp_http_client_is_chunked_response(evt->client))
        {
            ota_version_buffer = realloc(ota_version_buffer, ota_version_buffer_size + evt->data_len + 1);
            if (ota_version_buffer)
            {
                memcpy(ota_version_buffer + ota_version_buffer_size, evt->data, evt->data_len);
                ota_version_buffer_size += evt->data_len;
                ota_version_buffer[ota_version_buffer_size] = '\0';
            }
            else
            {
                ESP_LOGE(TAG, "get version: realloc failed");
            }
        }
        else
        {
            ESP_LOGI(TAG, "Chunked response");
        }
        break;
    }

    default:
        break;
    }
    return ESP_OK;
}