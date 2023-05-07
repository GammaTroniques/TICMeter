#include "wifi.h"

static int s_retry_num = 0;
uint8_t wifiConnected = 0;

/* FreeRTOS event group to signal when we are connected*/
static EventGroupHandle_t s_wifi_event_group;

uint8_t connectToWifi()
{
    static uint8_t firstCall = 1;
    s_retry_num = 0;
    ESP_LOGI(TAG, "FIRST WiFi INIT");

    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND)
    {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);

    s_wifi_event_group = xEventGroupCreate();
    ESP_ERROR_CHECK(esp_netif_init());

    if (firstCall)
    {
        firstCall = 0;
        ESP_ERROR_CHECK(esp_event_loop_create_default());
        esp_netif_create_default_wifi_sta();

        esp_event_handler_instance_t instance_any_id;
        esp_event_handler_instance_t instance_got_ip;
        ESP_ERROR_CHECK(esp_event_handler_instance_register(WIFI_EVENT,
                                                            ESP_EVENT_ANY_ID,
                                                            &event_handler,
                                                            NULL,
                                                            &instance_any_id));
        ESP_ERROR_CHECK(esp_event_handler_instance_register(IP_EVENT,
                                                            IP_EVENT_STA_GOT_IP,
                                                            &event_handler,
                                                            NULL,
                                                            &instance_got_ip));
    }

    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();

    wifi_config_t wifi_config = {
        .sta = {
            .ssid = EXAMPLE_ESP_WIFI_SSID,
            .password = EXAMPLE_ESP_WIFI_PASS,
            /* Authmode threshold resets to WPA2 as default if password matches WPA2 standards (pasword len => 8).
             * If you want to connect the device to deprecated WEP/WPA networks, Please set the threshold value
             * to WIFI_AUTH_WEP/WIFI_AUTH_WPA_PSK and set the password with length and format matching to
             * WIFI_AUTH_WEP/WIFI_AUTH_WPA_PSK standards.
             */
            .threshold = {
                .authmode = WIFI_AUTH_WPA_WPA2_PSK,
            },
            .sae_pwe_h2e = WPA3_SAE_PWE_HUNT_AND_PECK,
            .sae_h2e_identifier = "",
        },
    };
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));
    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_STA, &wifi_config));
    ESP_ERROR_CHECK(esp_wifi_start());

    ESP_LOGI(TAG, "wifi_init_sta finished. ");

    /* Waiting until either the connection is established (WIFI_CONNECTED_BIT) or connection failed for the maximum
     * number of re-tries (WIFI_FAIL_BIT). The bits are set by event_handler() (see above) */
    EventBits_t bits = xEventGroupWaitBits(s_wifi_event_group,
                                           WIFI_CONNECTED_BIT | WIFI_FAIL_BIT,
                                           pdFALSE,
                                           pdFALSE,
                                           portMAX_DELAY);

    /* xEventGroupWaitBits() returns the bits before the call returned, hence we can test which event actually
     * happened. */
    if (bits & WIFI_CONNECTED_BIT)
    {
        ESP_LOGI(TAG, "connected to ap SSID:%s ", EXAMPLE_ESP_WIFI_SSID);
        return 1;
    }
    else if (bits & WIFI_FAIL_BIT)
    {
        ESP_LOGI(TAG, "Failed to connect to SSID:%s", EXAMPLE_ESP_WIFI_SSID);
        return 0;
    }
    else
    {
        ESP_LOGE(TAG, "UNEXPECTED EVENT");
        return 0;
    }
}

void disconectFromWifi()
{
    esp_wifi_disconnect();
    esp_wifi_stop();
    esp_wifi_deinit();
    esp_wifi_set_ps(WIFI_PS_MAX_MODEM);
    ESP_LOGI(TAG, "wifi disconnected");
    // change cpu to 10Mhz
}

static void event_handler(void *arg, esp_event_base_t event_base,
                          int32_t event_id, void *event_data)
{
    if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_START)
    {
        esp_wifi_connect();
    }
    else if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_DISCONNECTED)
    {
        if (s_retry_num < EXAMPLE_ESP_MAXIMUM_RETRY)
        {
            esp_wifi_connect();
            s_retry_num++;
            ESP_LOGI(TAG, "retry to connect to the AP");
        }
        else
        {
            xEventGroupSetBits(s_wifi_event_group, WIFI_FAIL_BIT);
        }
        ESP_LOGI(TAG, "connect to the AP fail");
        wifiConnected = 0;
    }
    else if (event_base == IP_EVENT && event_id == IP_EVENT_STA_GOT_IP)
    {
        ip_event_got_ip_t *event = (ip_event_got_ip_t *)event_data;
        ESP_LOGI(TAG, "got ip:" IPSTR, IP2STR(&event->ip_info.ip));
        s_retry_num = 0;
        wifiConnected = 1;
        xEventGroupSetBits(s_wifi_event_group, WIFI_CONNECTED_BIT);
    }
}

//------------------

void createHttpUrl(char *url, const char *host, const char *path)
{
    url = strcat(url, "http://");
    url = strcat(url, host);
    url = strcat(url, path);
}

esp_err_t get_config_handler(esp_http_client_event_handle_t evt)
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

void getConfigFromServer(Config *config)
{
    ESP_LOGI(TAG, "get config from server");
    char url[100] = {0};
    createHttpUrl(url, config->values.web.host, config->values.web.configUrl);
    strcat(url, "?token=");
    strcat(url, config->values.web.token);
    ESP_LOGI(TAG, "url: %s", url);

    esp_http_client_config_t config_get = {
        .url = url,
        .cert_pem = NULL,
        .method = HTTP_METHOD_GET,
        .event_handler = get_config_handler,
    };

    esp_http_client_handle_t client = esp_http_client_init(&config_get);
    esp_http_client_perform(client);
    esp_http_client_cleanup(client);
}

time_t getTimestamp()
{
    static uint8_t firstCall = 1;
    if (firstCall)
    {
        firstCall = 0;
        esp_sntp_setoperatingmode(SNTP_OPMODE_POLL);
        esp_sntp_setservername(0, "pool.ntp.org");
        esp_sntp_init();
        time_t start = xTaskGetTickCount() / portTICK_PERIOD_MS;
        time_t noww;
        while (noww < 100000 && (xTaskGetTickCount() / portTICK_PERIOD_MS < start + 2000))
        {
            time(&noww);
            vTaskDelay(10 / portTICK_PERIOD_MS);
        }
        if (noww < 100000)
        {
            ESP_LOGE(TAG, "Failed to get time from NTP server");
            return 0;
        }
    }
    time_t now;
    time(&now);
    return now;
}

esp_err_t send_data_handler(esp_http_client_event_handle_t evt)
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

char sendToServer(char *json, Config *config)
{
    ESP_LOGI(TAG, "send data to server");
    char url[100] = {0};
    createHttpUrl(url, config->values.web.host, config->values.web.postUrl);
    ESP_LOGI(TAG, "url: %s", url);

    esp_http_client_config_t config_post = {
        .url = url,
        .cert_pem = NULL,
        .method = HTTP_METHOD_POST,
        .event_handler = send_data_handler,
    };

    esp_http_client_handle_t client = esp_http_client_init(&config_post);

    esp_http_client_set_post_field(client, json, strlen(json));
    esp_http_client_set_header(client, "Content-Type", "application/json");

    esp_http_client_perform(client);
    esp_http_client_cleanup(client);

    // #ifdef DEBUG
    //   Serial.print("Sending data to server... ");
    // #endif
    //   if (WiFi.status() == WL_CONNECTED)
    //   {
    //     WiFiClient client;
    //     HTTPClient http;

    //     char POST_URL[100] = {0};
    //     createHttpUrl(POST_URL, config.values.web.host, config.values.web.postUrl);
    //     http.begin(client, POST_URL);
    //     http.addHeader("Content-Type", "application/json");
    //     int httpCode = http.POST(json);
    // #ifdef DEBUG
    //     Serial.print("OK: ");
    //     Serial.println(httpCode);
    // #endif
    //     http.end();
    //     return httpCode;
    //   }
    // #ifdef DEBUG
    //   Serial.print("ERROR");
    // #endif
    return -1;
}
