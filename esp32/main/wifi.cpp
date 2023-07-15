#include "wifi.h"
#include "http.h"
#include "gpio.h"
#include "dns_server.h"
#include "gpio.h"
#include "driver/gpio.h"
#include "esp_sntp.h"
#include "esp_netif_sntp.h"

#define TAG "WIFI"

static int s_retry_num = 0;
uint8_t wifiConnected = 0;
#ifndef MAC2STR
#define MAC2STR(a) (a)[0], (a)[1], (a)[2], (a)[3], (a)[4], (a)[5]
#define MACSTR "%02x:%02x:%02x:%02x:%02x:%02x"
#endif

#define NTP_SERVER "pool.ntp.org"

/* FreeRTOS event group to signal when we are connected*/
static EventGroupHandle_t s_wifi_event_group;

esp_netif_t *sta_netif = NULL;
esp_event_handler_instance_t instance_any_id;
esp_event_handler_instance_t instance_got_ip;
uint8_t connectToWifi()
{
    esp_log_level_set("wifi", ESP_LOG_ERROR);
    esp_log_level_set("wifi_init", ESP_LOG_ERROR);
    esp_log_level_set("esp_adapter", ESP_LOG_ERROR);
    esp_log_level_set("pp", ESP_LOG_ERROR);
    esp_log_level_set("net80211", ESP_LOG_ERROR);
    esp_log_level_set("esp_netif_handlers", ESP_LOG_ERROR);
    esp_log_level_set("phy_version", ESP_LOG_ERROR);
    if (wifiConnected)
        return 1;

    if (strlen(config.values.ssid) == 0 || strlen(config.values.password) == 0)
    {
        ESP_LOGI(TAG, "No Wifi SSID or password");
        return 0;
    }

    startLedPattern(PATTERN_WIFI_CONNECTING);

    s_retry_num = 0;
    esp_wifi_set_ps(WIFI_PS_NONE);
    s_wifi_event_group = xEventGroupCreate();

    ESP_ERROR_CHECK(esp_netif_init());

    ESP_ERROR_CHECK(esp_event_loop_create_default());
    sta_netif = esp_netif_create_default_wifi_sta();

    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));

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

    wifi_config_t wifi_config = {};

    wifi_config.sta.threshold.authmode = WIFI_AUTH_WPA_WPA2_PSK;
    wifi_config.sta.sae_pwe_h2e = WPA3_SAE_PWE_HUNT_AND_PECK;
    wifi_config.sta.sae_h2e_identifier[0] = '\0';

    strcpy((char *)wifi_config.sta.ssid, config.values.ssid);
    strcpy((char *)wifi_config.sta.password, config.values.password);

    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_STA, &wifi_config));
    ESP_ERROR_CHECK(esp_wifi_start());

    /* Waiting until either the connection is established (WIFI_CONNECTED_BIT) or connection failed for the maximum
     * number of re-tries (WIFI_FAIL_BIT). The bits are set by event_handler() (see above) */
    ESP_LOGI(TAG, "Connecting to %s", (char *)wifi_config.sta.ssid);
    EventBits_t bits = xEventGroupWaitBits(s_wifi_event_group,
                                           WIFI_CONNECTED_BIT | WIFI_FAIL_BIT,
                                           pdFALSE,
                                           pdFALSE,
                                           5000 / portTICK_PERIOD_MS);

    /* xEventGroupWaitBits() returns the bits before the call returned, hence we can test which event actually
     * happened. */
    if (bits & WIFI_CONNECTED_BIT)
    {
        ESP_LOGI(TAG, "Connected to ap SSID:%s", (char *)wifi_config.sta.ssid);
    }
    else if (bits & WIFI_FAIL_BIT)
    {
        ESP_LOGI(TAG, "Failed to connect to SSID:%s", (char *)wifi_config.sta.ssid);
        // disconectFromWifi();
        return 0;
    }
    else
    {
        ESP_LOGE(TAG, "UNEXPECTED EVENT");
        disconectFromWifi();
        return 0;
    }
    return 1;
}

void disconectFromWifi()
{
    if (!wifiConnected)
    {
        ESP_LOGI(TAG, "wifi already not connected");
        return;
    }
    wifiConnected = 0;
    ESP_LOGI(TAG, "Disconnected");
    esp_wifi_disconnect();
    esp_wifi_stop();
    esp_wifi_deinit();
    vEventGroupDelete(s_wifi_event_group);
    // ESP_ERROR_CHECK(esp_event_handler_instance_unregister(IP_EVENT, IP_EVENT_STA_GOT_IP, instance_got_ip));
    // ESP_ERROR_CHECK(esp_event_handler_instance_unregister(WIFI_EVENT, ESP_EVENT_ANY_ID, instance_any_id));

    ESP_ERROR_CHECK(esp_event_loop_delete_default());
    ESP_ERROR_CHECK(esp_wifi_clear_default_wifi_driver_and_handlers(sta_netif));

    // esp_netif_t *netif = esp_netif_get_default_netif();
    // esp_netif_destroy_default_wifi(netif);
    // ESP_ERROR_CHECK(esp_netif_deinit());
    esp_netif_destroy(sta_netif);
    esp_wifi_set_ps(WIFI_PS_MAX_MODEM);
    // change cpu to 10Mhz

    // ESP_ERROR_CHECK(esp_wifi_stop());
}

void event_handler(void *arg, esp_event_base_t event_base,
                   int32_t event_id, void *event_data)
{

    // ESP_LOGI(TAG, "GOT EVENT: event_base: %s, event_id: %ld", event_base, event_id);
    if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_START)
    {
        esp_wifi_connect();
    }
    else if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_DISCONNECTED && wifiConnected == 1)
    {
        if (s_retry_num < ESP_MAXIMUM_RETRY)
        {
            esp_wifi_connect();
            s_retry_num++;
            ESP_LOGI(TAG, "Retry to connect to the AP: %d/%d", s_retry_num, ESP_MAXIMUM_RETRY);
            startLedPattern(PATTERN_WIFI_RETRY);
        }
        else
        {
            xEventGroupSetBits(s_wifi_event_group, WIFI_FAIL_BIT);
            ESP_LOGI(TAG, "Connect to the AP fail");
            startLedPattern(PATTERN_WIFI_FAILED);
        }
        wifiConnected = 0;
    }
    else if (event_base == IP_EVENT && event_id == IP_EVENT_STA_GOT_IP)
    {
        ip_event_got_ip_t *event = (ip_event_got_ip_t *)event_data;
        ESP_LOGI(TAG, "IP:" IPSTR, IP2STR(&event->ip_info.ip));
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
    config_get.method = HTTP_METHOD_GET;
    config_get.event_handler = get_config_handler;

    esp_http_client_handle_t client = esp_http_client_init(&config_get);
    esp_http_client_perform(client);
    esp_http_client_cleanup(client);
}

void time_sync_notification_cb(struct timeval *tv)
{
    // ESP_LOGI(TAG, "Notification of a time synchronization event");
}

uint8_t sntpInit()
{

    // esp_sntp_setoperatingmode(SNTP_OPMODE_POLL);
    // esp_sntp_setservername(0, "pool.ntp.org");
    // sntp_set_time_sync_notification_cb(time_sync_notification_cb);
    // esp_sntp_init();
    // time_t timeout = MILLIS + 3000;
    // while (sntp_get_sync_status() == SNTP_SYNC_STATUS_RESET && (MILLIS < timeout))
    // {
    //     vTaskDelay(100 / portTICK_PERIOD_MS);
    // }
    // if (sntp_get_sync_status() == SNTP_SYNC_STATUS_RESET)
    // {
    //     ESP_LOGE(TAG, "Failed to get time from NTP server");
    //     return 0;
    // }

    return 1;
}

time_t getTimestamp()
{
    static time_t now = 0;
    struct tm timeinfo;
    if (wifiConnected)
    {
        esp_sntp_config_t config = ESP_NETIF_SNTP_DEFAULT_CONFIG(NTP_SERVER);
        config.sync_cb = time_sync_notification_cb; // Note: This is only needed if we want
        esp_netif_sntp_init(&config);

        int retry = 0;
        sntp_set_sync_status(SNTP_SYNC_STATUS_RESET);
        esp_err_t err;
        do
        {
            err = esp_netif_sntp_sync_wait(2000 / portTICK_PERIOD_MS);
            // ESP_LOGI(TAG, "Waiting for system time to be set... (%d/%d) %d", retry, 3, err);
        } while (err == ESP_ERR_TIMEOUT && ++retry < 2);

        // time_t now = 0;
        // time(&now);
        // time_t timeout = MILLIS + 3000;
        // while (sntp_get_sync_status() == SNTP_SYNC_STATUS_RESET && (MILLIS < timeout))
        // {
        //     vTaskDelay(100 / portTICK_PERIOD_MS);
        // }
        if (sntp_get_sync_status() == SNTP_SYNC_STATUS_RESET)
        {
            ESP_LOGE(TAG, "Failed to get time from NTP server, return last time");
        }
        esp_netif_sntp_deinit();
    }
    time(&now);
    localtime_r(&now, &timeinfo);
    char strftime_buf[64];
    strftime(strftime_buf, sizeof(strftime_buf), "%c", &timeinfo);
    ESP_LOGI(TAG, "The current date/time is: %s", strftime_buf);
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

uint8_t sendToServer(const char *json)
{
    if (strlen(config.values.web.host) == 0 || strlen(config.values.web.postUrl) == 0)
    {
        ESP_LOGE(TAG, "host or postUrl not set");
        return 0;
    }
    char url[100] = {0};
    createHttpUrl(url, config.values.web.host, config.values.web.postUrl);
    // setup post request
    esp_http_client_config_t config_post;
    memset(&config_post, 0, sizeof(config_post));
    config_post.url = url;
    config_post.cert_pem = NULL;
    config_post.method = HTTP_METHOD_POST;
    config_post.event_handler = send_data_handler;

    // setup client
    esp_http_client_handle_t client = esp_http_client_init(&config_post);
    esp_http_client_set_post_field(client, json, strlen(json));
    esp_http_client_set_header(client, "Content-Type", "application/json");

    // send post request
    esp_http_client_perform(client);
    esp_http_client_cleanup(client);
    return 1;
}

static void wifi_init_softap(void)
{
    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));

    ESP_ERROR_CHECK(esp_event_handler_register(WIFI_EVENT, ESP_EVENT_ANY_ID, &event_handler, NULL));

    wifi_config_t wifi_config = {};
    strcpy((char *)wifi_config.ap.ssid, AP_SSID);
    strcpy((char *)wifi_config.ap.password, AP_PASS);
    wifi_config.ap.ssid_len = strlen(AP_SSID);
    wifi_config.ap.authmode = WIFI_AUTH_WPA_WPA2_PSK;
    wifi_config.ap.max_connection = 4;
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

void stop_captive_portal_task(void *pvParameter)
{
    uint8_t readCount = 0;

    while (1)
    {
        if (!getVUSB())
        {
            readCount++;
        }
        else
        {
            readCount = 0;
        }
        if (readCount > 3)
        {
            ESP_LOGI(TAG, "VUSB is not connected, stop captive portal");
            esp_restart();
        }
        gpio_set_level(LED_GREEN, 1);
        vTaskDelay(50 / portTICK_PERIOD_MS);
        gpio_set_level(LED_GREEN, 0);
        vTaskDelay(1000 / portTICK_PERIOD_MS);
    }
}

void start_captive_portal()
{
    xTaskCreate(&stop_captive_portal_task, "stop_captive_portal_task", 2048, NULL, 1, NULL);
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
