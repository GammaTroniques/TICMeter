#include "wifi.h"
#include "http.h"
#include "dns_server.h"

static int s_retry_num = 0;
uint8_t wifiConnected = 0;
#ifndef MAC2STR
#define MAC2STR(a) (a)[0], (a)[1], (a)[2], (a)[3], (a)[4], (a)[5]
#define MACSTR "%02x:%02x:%02x:%02x:%02x:%02x"
#endif
/* FreeRTOS event group to signal when we are connected*/
static EventGroupHandle_t s_wifi_event_group;

esp_netif_t *sta_netif = NULL;

uint8_t connectToWifi()
{
    s_retry_num = 0;
    esp_wifi_set_ps(WIFI_PS_NONE);
    s_wifi_event_group = xEventGroupCreate();

    ESP_ERROR_CHECK(esp_netif_init());

    ESP_ERROR_CHECK(esp_event_loop_create_default());
    sta_netif = esp_netif_create_default_wifi_sta();

    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));

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

    wifi_config_t wifi_config;
    memset(&wifi_config, 0, sizeof(wifi_config_t));

    wifi_config.sta.threshold.authmode = WIFI_AUTH_WPA_WPA2_PSK;
    wifi_config.sta.sae_pwe_h2e = WPA3_SAE_PWE_HUNT_AND_PECK;
    wifi_config.sta.sae_h2e_identifier[0] = '\0';

    strcpy((char *)wifi_config.sta.ssid, config.values.ssid);
    strcpy((char *)wifi_config.sta.password, config.values.password);

    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_STA, &wifi_config));
    ESP_ERROR_CHECK(esp_wifi_start());

    ESP_LOGI(TAG, "wifi_init_sta finished.");

    /* Waiting until either the connection is established (WIFI_CONNECTED_BIT) or connection failed for the maximum
     * number of re-tries (WIFI_FAIL_BIT). The bits are set by event_handler() (see above) */
    ESP_LOGI(TAG, "Waiting for wifi");
    EventBits_t bits = xEventGroupWaitBits(s_wifi_event_group,
                                           WIFI_CONNECTED_BIT | WIFI_FAIL_BIT,
                                           pdFALSE,
                                           pdFALSE,
                                           10000 / portTICK_PERIOD_MS);

    /* xEventGroupWaitBits() returns the bits before the call returned, hence we can test which event actually
     * happened. */
    ESP_LOGI(TAG, "Got wifi");
    if (bits & WIFI_CONNECTED_BIT)
    {
        ESP_LOGI(TAG, "connected to ap SSID:%s", (char *)wifi_config.sta.ssid);
    }
    else if (bits & WIFI_FAIL_BIT)
    {
        ESP_LOGI(TAG, "Failed to connect to SSID:%s", (char *)wifi_config.sta.ssid);
    }
    else
    {
        ESP_LOGE(TAG, "UNEXPECTED EVENT");
    }

    initi_web_page_buffer();
    setup_server();
    return 0;
}

void disconectFromWifi()
{
    ESP_LOGI(TAG, "wifi disconnected");
    esp_wifi_disconnect();
    esp_wifi_stop();
    esp_wifi_deinit();
    vEventGroupDelete(s_wifi_event_group);
    // ESP_ERROR_CHECK(esp_event_handler_instance_unregister(IP_EVENT, IP_EVENT_STA_GOT_IP, instance_got_ip));
    // ESP_ERROR_CHECK(esp_event_handler_instance_unregister(WIFI_EVENT, ESP_EVENT_ANY_ID, instance_any_id));

    ESP_ERROR_CHECK(esp_event_loop_delete_default());
    // esp_netif_t *netif = esp_netif_get_default_netif();
    // esp_netif_destroy_default_wifi(netif);
    // ESP_ERROR_CHECK(esp_netif_deinit());
    esp_netif_destroy(sta_netif);
    esp_wifi_set_ps(WIFI_PS_MAX_MODEM);
    // change cpu to 10Mhz
}

static void event_handler(void *arg, esp_event_base_t event_base,
                          int32_t event_id, void *event_data)
{

    ESP_LOGI(TAG, "GOT EVENT: event_base: %s, event_id: %ld", event_base, event_id);
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
    else if (event_id == WIFI_EVENT_AP_STACONNECTED)
    {
        wifi_event_ap_staconnected_t *event = (wifi_event_ap_staconnected_t *)event_data;
        ESP_LOGI(TAG, "station " MACSTR " join, AID=%d",
                 MAC2STR(event->mac), event->aid);
    }
    else if (event_id == WIFI_EVENT_AP_STADISCONNECTED)
    {
        wifi_event_ap_stadisconnected_t *event = (wifi_event_ap_stadisconnected_t *)event_data;
        ESP_LOGI(TAG, "station " MACSTR " leave, AID=%d",
                 MAC2STR(event->mac), event->aid);
    }
    else
    {
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

    esp_http_client_config_t config_get;
    memset(&config_get, 0, sizeof(config_get));
    config_get.url = url;
    config_get.cert_pem = NULL;
    config_get.method = HTTP_METHOD_POST;
    config_get.event_handler = get_config_handler;

    esp_http_client_handle_t client = esp_http_client_init(&config_get);
    esp_http_client_perform(client);
    esp_http_client_cleanup(client);
}

time_t getTimestamp()
{

    static uint8_t firstCall = 1;
    if (firstCall)
    {
        // if (!wifiConnected)
        // {
        //     ESP_LOGE(TAG, "wifi not connected");
        //     return 0;
        // }
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
    if (strcmp(config->values.web.host, "") == 0 || strcmp(config->values.web.postUrl, "") == 0)
    {
        ESP_LOGE(TAG, "host or postUrl not set");
        return 0;
    }

    ESP_LOGI(TAG, "send data to server");
    char url[100] = {0};
    createHttpUrl(url, config->values.web.host, config->values.web.postUrl);
    ESP_LOGI(TAG, "url: %s", url);

    esp_http_client_config_t config_post;
    memset(&config_post, 0, sizeof(config_post));
    config_post.url = url;
    config_post.cert_pem = NULL;
    config_post.method = HTTP_METHOD_POST;
    config_post.event_handler = send_data_handler;

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

static void wifi_init_softap(void)
{
    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));

    ESP_ERROR_CHECK(esp_event_handler_register(WIFI_EVENT, ESP_EVENT_ANY_ID, &event_handler, NULL));

    wifi_config_t wifi_config = {
        .ap = {
            .ssid = AP_SSID,
            .password = AP_PASS,
            .ssid_len = strlen(AP_SSID),
            .authmode = WIFI_AUTH_WPA_WPA2_PSK,
            .max_connection = 4,
        }};

    if (strlen(AP_PASS) == 0)
    {
        wifi_config.ap.authmode = WIFI_AUTH_OPEN;
    }

    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_AP));
    ESP_ERROR_CHECK(esp_wifi_set_config((wifi_interface_t)ESP_IF_WIFI_AP, &wifi_config));
    ESP_ERROR_CHECK(esp_wifi_start());

    esp_netif_ip_info_t ip_info;
    esp_netif_get_ip_info(esp_netif_get_handle_from_ifkey("WIFI_AP_DEF"), &ip_info);

    char ip_addr[16];
    inet_ntoa_r(ip_info.ip.addr, ip_addr, 16);
    ESP_LOGI(TAG, "Set up softAP with IP: %s", ip_addr);

    ESP_LOGI(TAG, "wifi_init_softap finished. SSID:'%s' password:'%s'",
             AP_SSID, AP_PASS);
}

void start_captive_portal()
{
    // Initialize networking stack
    ESP_ERROR_CHECK(esp_netif_init());

    // Create default event loop needed by the  main app
    ESP_ERROR_CHECK(esp_event_loop_create_default());

    // Initialize NVS needed by Wi-Fi
    ESP_ERROR_CHECK(nvs_flash_init());

    // Initialize Wi-Fi including netif with default config
    esp_netif_create_default_wifi_ap();

    // Initialise ESP32 in SoftAP mode
    wifi_init_softap();

    initi_web_page_buffer();
    // Start the server for the first time
    setup_server();

    // Start the DNS server that will redirect all queries to the softAP IP
    start_dns_server();
}