#include "wifi.h"
#include "http.h"
#include "gpio.h"
#include "dns_server.h"
#include "gpio.h"
#include "driver/gpio.h"
#include "esp_sntp.h"
#include <time.h>
#include "esp_netif_sntp.h"

#define TAG "WIFI"

static int s_retry_num = 0;
uint8_t wifiConnected = 0;
uint8_t sendingValues = 0;

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
    esp_err_t err = ESP_OK;
    if (wifiConnected)
        return 1;

    if (strlen(config.values.ssid) == 0 || strlen(config.values.password) == 0)
    {
        ESP_LOGI(TAG, "No Wifi SSID or password");
        return 0;
    }
    xTaskCreate(gpio_led_task_wifi_connecting, "gpio_led_task_wifi_connecting", 4096, NULL, 1, NULL); // start wifi connect led task
    s_retry_num = 0;
    esp_wifi_set_ps(WIFI_PS_MAX_MODEM);
    s_wifi_event_group = xEventGroupCreate();

    err = esp_netif_init();
    if (err != ESP_OK)
    {
        ESP_LOGE(TAG, "esp_netif_init failed with 0x%X", err);
        wifiConnected = 2; // error
        disconnectFromWifi();
        return 0;
    }

    err = esp_event_loop_create_default();
    switch (err)
    {
    case ESP_OK:
        break;
    case ESP_ERR_INVALID_STATE:
        ESP_LOGW(TAG, "esp_event_loop_create_default failed with 0x%X: possibly already exist", err);
        wifiConnected = 2; // error
        disconnectFromWifi();
        return 0;
        break;
    default:
        ESP_LOGE(TAG, "esp_event_loop_create_default failed with 0x%X", err);
        wifiConnected = 2; // error
        disconnectFromWifi();
        return 0;
        break;
    }

    sta_netif = esp_netif_create_default_wifi_sta();

    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    err = esp_wifi_init(&cfg);
    if (err != ESP_OK)
    {
        ESP_LOGE(TAG, "esp_wifi_init failed with 0x%X", err);
        wifiConnected = 2; // error
        disconnectFromWifi();
        return 0;
    }

    err = esp_event_handler_instance_register(WIFI_EVENT,
                                              ESP_EVENT_ANY_ID,
                                              &event_handler,
                                              NULL,
                                              &instance_any_id);
    if (err != ESP_OK)
    {
        ESP_LOGE(TAG, "esp_event_handler_instance_register failed with 0x%X", err);
        wifiConnected = 2; // error
        disconnectFromWifi();
        return 0;
    }

    err = esp_event_handler_instance_register(IP_EVENT,
                                              IP_EVENT_STA_GOT_IP,
                                              &event_handler,
                                              NULL,
                                              &instance_got_ip);
    if (err != ESP_OK)
    {
        ESP_LOGE(TAG, "esp_event_handler_instance_register failed with 0x%X", err);
        wifiConnected = 2; // error
        disconnectFromWifi();
        return 0;
    }

    wifi_config_t wifi_config = {};

    wifi_config.sta.threshold.authmode = WIFI_AUTH_WPA_WPA2_PSK;
    wifi_config.sta.sae_pwe_h2e = WPA3_SAE_PWE_HUNT_AND_PECK;
    wifi_config.sta.sae_h2e_identifier[0] = '\0';

    strncpy((char *)wifi_config.sta.ssid, config.values.ssid, sizeof(wifi_config.sta.ssid));
    strncpy((char *)wifi_config.sta.password, config.values.password, sizeof(wifi_config.sta.password));

    err = esp_wifi_set_mode(WIFI_MODE_STA);
    if (err != ESP_OK)
    {
        ESP_LOGE(TAG, "esp_wifi_set_mode failed with 0x%X", err);
        wifiConnected = 2; // error
        disconnectFromWifi();
        return 0;
    }
    err = esp_wifi_set_config(WIFI_IF_STA, &wifi_config);
    if (err != ESP_OK)
    {
        ESP_LOGE(TAG, "esp_wifi_set_config failed with 0x%X", err);
        wifiConnected = 2; // error
        disconnectFromWifi();
        return 0;
    }
    err = esp_wifi_start();
    if (err != ESP_OK)
    {
        ESP_LOGE(TAG, "esp_wifi_start failed with 0x%X", err);
        wifiConnected = 2; // error
        disconnectFromWifi();
        return 0;
    }

    ESP_LOGI(TAG, "Connecting to %s", (char *)wifi_config.sta.ssid);
retry:
    EventBits_t bits = xEventGroupWaitBits(s_wifi_event_group,
                                           WIFI_CONNECTED_BIT | WIFI_FAIL_BIT,
                                           pdFALSE,
                                           pdFALSE,
                                           WIFI_CONNECT_TIMEOUT / portTICK_PERIOD_MS);

    /* xEventGroupWaitBits() returns the bits before the call returned, hence we can test which event actually
     * happened. */
    if (bits & WIFI_CONNECTED_BIT)
    {
        ESP_LOGI(TAG, "Connected to ap SSID:%s", (char *)wifi_config.sta.ssid);
        return 1;
    }
    else if (bits & WIFI_FAIL_BIT)
    {
        ESP_LOGI(TAG, "Failed to connect to SSID:%s", (char *)wifi_config.sta.ssid);
        disconnectFromWifi();
        return 0;
    }
    else
    {
        ESP_LOGE(TAG, "UNEXPECTED EVENT: Timeout");
        goto retry;
        // esp_wifi_deinit();
        // return 0;
    }
}

void disconnectFromWifi()
{
    esp_err_t err = ESP_OK;
    if (wifiConnected == 0) // already disconnected
    {
        ESP_LOGD(TAG, "wifi already not connected");
        return;
    }
    wifiConnected = 0;
    ESP_LOGI(TAG, "Disconnected");

    err = esp_event_handler_instance_unregister(IP_EVENT, IP_EVENT_STA_GOT_IP, instance_got_ip);
    if (err != ESP_OK)
    {
        ESP_LOGE(TAG, "esp_event_handler_instance_unregister failed with 0x%X", err);
    }
    err = esp_event_handler_instance_unregister(WIFI_EVENT, ESP_EVENT_ANY_ID, instance_any_id);
    if (err != ESP_OK)
    {
        ESP_LOGE(TAG, "esp_event_handler_instance_unregister failed with 0x%X", err);
    }

    err = esp_wifi_disconnect();
    if (err != ESP_OK)
    {
        ESP_LOGE(TAG, "esp_wifi_disconnect failed with 0x%X", err);
    }
    err = esp_wifi_stop();
    if (err != ESP_OK)
    {
        ESP_LOGE(TAG, "esp_wifi_stop failed with 0x%X", err);
    }
    err = esp_wifi_deinit();
    if (err != ESP_OK)
    {
        ESP_LOGE(TAG, "esp_wifi_deinit failed with 0x%X", err);
    }

    vEventGroupDelete(s_wifi_event_group);
    err = esp_event_loop_delete_default();
    if (err != ESP_OK)
    {
        ESP_LOGE(TAG, "esp_event_loop_delete_default failed with 0x%X", err);
    }

    err = esp_wifi_clear_default_wifi_driver_and_handlers(sta_netif);
    if (err != ESP_OK)
    {
        ESP_LOGE(TAG, "esp_wifi_clear_default_wifi_driver_and_handlers failed with 0x%X", err);
    }

    esp_netif_destroy(sta_netif);
    esp_wifi_set_ps(WIFI_PS_MAX_MODEM);
}

void event_handler(void *arg, esp_event_base_t event_base, int32_t event_id, void *event_data)
{

    // ESP_LOGI(TAG, "GOT EVENT: event_base: %s, event_id: %ld", event_base, event_id);
    if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_START)
    {
        wifiConnected = 0;
        esp_wifi_connect();
    }
    else if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_DISCONNECTED) // && wifiConnected == 1)
    {
        wifiConnected = 0;
        if (s_retry_num < ESP_MAXIMUM_RETRY)
        {
            esp_wifi_connect();
            s_retry_num++;
            ESP_LOGW(TAG, "Retry to connect to the AP: %d/%d", s_retry_num, ESP_MAXIMUM_RETRY);
        }
        else
        {
            xEventGroupSetBits(s_wifi_event_group, WIFI_FAIL_BIT);
            ESP_LOGE(TAG, "Connect to the AP fail");
            gpio_start_led_pattern(PATTERN_WIFI_FAILED);
        }
        wifiConnected = 0;
    }
    else if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_CONNECTED)
    {
        ESP_LOGI(TAG, "Connected");
        wifiConnected = 1;
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
        static bool sntp_started = false;
        static esp_sntp_config_t config = ESP_NETIF_SNTP_DEFAULT_CONFIG(NTP_SERVER);

        if (!sntp_started)
        {
            sntp_started = true;
            config.sync_cb = time_sync_notification_cb; // Note: This is only needed if we want
            sntp_set_sync_interval(0);                  // sync now
            esp_netif_sntp_init(&config);
        }
        else
        {
            sntp_restart();
        }
        // sntp_set_sync_status(SNTP_SYNC_STATUS_RESET);
        int retry = 0;
        esp_err_t err;
        do
        {
            err = esp_netif_sntp_sync_wait(2000 / portTICK_PERIOD_MS);
            // ESP_LOGI(TAG, "Waiting for system time to be set... (%d/%d) %d", retry, 3, err);
        } while (err == ESP_ERR_TIMEOUT && ++retry < 2);

        if (sntp_get_sync_status() == SNTP_SYNC_STATUS_RESET)
        {
            ESP_LOGE(TAG, "Failed to get time from NTP server, return last time");
        }
        // esp_netif_sntp_deinit();
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
    sendingValues = 1;
    xTaskCreate(gpio_led_task_sending, "gpio_led_task_sending", 2048, NULL, 1, NULL);

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
    sendingValues = 0;
    return 1;
}

static void wifi_init_softap(void)
{
    // Initialize Wi-Fi including netif with default config
    esp_netif_t *wifiAP = esp_netif_create_default_wifi_ap();
    esp_netif_ip_info_t ip_info;
    IP4_ADDR(&ip_info.ip, 4, 3, 2, 1);
    IP4_ADDR(&ip_info.gw, 4, 3, 2, 1);
    IP4_ADDR(&ip_info.netmask, 255, 255, 255, 0);
    esp_netif_dhcps_stop(wifiAP);
    esp_netif_set_ip_info(wifiAP, &ip_info);
    esp_netif_dhcps_start(wifiAP);

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

    // esp_netif_get_ip_info(esp_netif_get_handle_from_ifkey("WIFI_AP_DEF"), &ip_info);
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
        if (gpio_get_vusb() < 3)
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
        // gpio_set_level(LED_GREEN, 1);
        // vTaskDelay(50 / portTICK_PERIOD_MS);
        // gpio_set_level(LED_GREEN, 0);
        vTaskDelay(1000 / portTICK_PERIOD_MS);
    }
}

void start_captive_portal()
{
    ESP_LOGI(TAG, "Start captive portal");
    // xTaskCreate(&stop_captive_portal_task, "stop_captive_portal_task", 2048, NULL, 1, NULL);
    // Initialize networking stack
    ESP_ERROR_CHECK(esp_netif_init());

    // Create default event loop needed by the  main app
    ESP_ERROR_CHECK(esp_event_loop_create_default());

    // Initialize NVS needed by Wi-Fi
    ESP_ERROR_CHECK(nvs_flash_init());

    // Initialise ESP32 in SoftAP mode
    wifi_init_softap();

    initi_web_page_buffer();
    // Start the server for the first time
    setup_server();
    // Start the DNS server that will redirect all queries to the softAP IP
    start_dns_server();
}
