/**
 * @file wifi.cpp
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
#include "wifi.h"
#include "http.h"
#include "gpio.h"
#include "led.h"
#include "dns_server.h"
#include "gpio.h"
#include "driver/gpio.h"
#include "esp_sntp.h"
#include <time.h>
#include "esp_netif_sntp.h"
#include "esp_mac.h"
#include "esp_private/periph_ctrl.h"
#include "soc/periph_defs.h"
#include "esp_sleep.h"
#include "ping/ping_sock.h"
/*==============================================================================
 Local Define
===============================================================================*/
#define TAG "WIFI"
#define NTP_SERVER "pool.ntp.org"

#define ESP_MAXIMUM_RETRY 4
#define WIFI_CONNECT_FAIL_COUNT_BEFORE_RESET 10
#define WIFI_CONNECTED_BIT BIT0
#define WIFI_FAIL_BIT BIT1
#define WIFI_AUTHFAIL_BIT BIT2
#define WIFI_NO_AP_FOUND_BIT BIT3

/*==============================================================================
 Local Macro
===============================================================================*/
#ifndef MAC2STR
#define MAC2STR(a) (a)[0], (a)[1], (a)[2], (a)[3], (a)[4], (a)[5]
#define MACSTR "%02x:%02x:%02x:%02x:%02x:%02x"
#endif
/*==============================================================================
 Local Type
===============================================================================*/

/*==============================================================================
 Local Function Declaration
===============================================================================*/
static void wifi_event_handler(void *arg, esp_event_base_t event_base, int32_t event_id, void *event_data);
static void wifi_time_sync_notification_cb(struct timeval *tv);
static void stop_captive_portal_task(void *pvParameter);

/*==============================================================================
Public Variable
===============================================================================*/
wifi_state_t wifi_state = WIFI_DISCONNECTED;

uint32_t wifi_timeout_counter = 0;
wifi_ap_record_t wifi_ap_list[20];

esp_netif_ip_info_t wifi_current_ip;

/*==============================================================================
 Local Variable
===============================================================================*/
static int s_retry_num = 0;
static EventGroupHandle_t s_wifi_event_group;
static EventGroupHandle_t ping_event_group;
static uint8_t ap_started = 0;

static uint32_t ping_time_array[10];
static uint32_t ping_index = 0;
/*==============================================================================
Function Implementation
===============================================================================*/

uint8_t wifi_init()
{
    esp_err_t err = ESP_OK;
    err = esp_netif_init();
    if (err != ESP_OK)
    {
        ESP_LOGE(TAG, "esp_netif_init failed with 0x%X", err);
        return 0;
    }
    err = esp_event_loop_create_default();
    if (err != ESP_OK)
    {
        ESP_LOGE(TAG, "esp_event_loop_create_default failed with 0x%X", err);
        return 0;
    }
    esp_netif_t *netif = esp_netif_create_default_wifi_sta();

    err = esp_netif_set_hostname(netif, HOSTNAME);
    if (err != ESP_OK)
    {
        ESP_LOGW(TAG, "Failed to set hostname: 0x%X", err);
    }

    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    err = esp_wifi_init(&cfg);
    if (err != ESP_OK)
    {
        ESP_LOGE(TAG, "esp_wifi_init failed with 0x%X", err);
        return 0;
    }

    esp_event_handler_instance_t instance_any_id;
    esp_event_handler_instance_t instance_got_ip;

    err = esp_event_handler_instance_register(WIFI_EVENT, ESP_EVENT_ANY_ID, &wifi_event_handler, NULL, &instance_any_id);
    if (err != ESP_OK)
    {
        ESP_LOGE(TAG, "esp_event_handler_instance_register failed with 0x%X", err);
        return 0;
    }
    err = esp_event_handler_instance_register(IP_EVENT, IP_EVENT_STA_GOT_IP, &wifi_event_handler, NULL, &instance_got_ip);
    if (err != ESP_OK)
    {
        ESP_LOGE(TAG, "esp_event_handler_instance_register failed with 0x%X", err);
        return 0;
    }
    s_wifi_event_group = xEventGroupCreate();
    ping_event_group = xEventGroupCreate();
    return 1;
}

esp_err_t wifi_connect()
{
    esp_err_t err = ESP_OK;
    if (wifi_state == WIFI_CONNECTED) // already connected
        return ESP_OK;

    if (strlen(config_values.ssid) == 0 || strlen(config_values.password) == 0)
    {
        ESP_LOGI(TAG, "No Wifi SSID or password");
        return ESP_ERR_WIFI_MODE;
    }
    if (wifi_timeout_counter > WIFI_CONNECT_FAIL_COUNT_BEFORE_RESET)
    {
        ESP_LOGE(TAG, "Too many wifi timeout (%ld): Hard reset", wifi_timeout_counter);
        gpio_set_direction(GPIO_NUM_15, GPIO_MODE_OUTPUT);
        gpio_set_level(GPIO_NUM_15, 0);
        return ESP_ERR_WIFI_STATE;
    }

    wifi_state = WIFI_CONNECTING;
    led_start_pattern(LED_CONNECTING);

    s_retry_num = 0;
    xEventGroupClearBits(s_wifi_event_group, WIFI_CONNECTED_BIT | WIFI_FAIL_BIT | WIFI_AUTHFAIL_BIT | WIFI_NO_AP_FOUND_BIT);

    wifi_config_t wifi_config = {
        .sta = {
            .threshold = {
                .authmode = WIFI_AUTH_WPA_WPA2_PSK,
            },
            .sae_h2e_identifier = {0},
            .sae_pwe_h2e = WPA3_SAE_PWE_HUNT_AND_PECK,
        },
    };

    strncpy((char *)wifi_config.sta.ssid, config_values.ssid, sizeof(wifi_config.sta.ssid));
    strncpy((char *)wifi_config.sta.password, config_values.password, sizeof(wifi_config.sta.password));

    err = esp_wifi_set_ps(WIFI_PS_NONE);
    if (err != ESP_OK)
    {
        ESP_LOGE(TAG, "esp_wifi_set_ps failed with 0x%X", err);
    }

    wifi_mode_t mode = WIFI_MODE_STA;
    if (ap_started)
    {
        ESP_LOGI(TAG, "wifi_connect: APSTA mode");
        mode = WIFI_MODE_APSTA;
    }
    err = esp_wifi_set_mode(mode);
    if (err != ESP_OK)
    {
        ESP_LOGE(TAG, "esp_wifi_set_mode failed with 0x%X", err);
        wifi_state = WIFI_FAILED;
        wifi_disconnect();
        return err;
    }
    err = esp_wifi_set_config(WIFI_IF_STA, &wifi_config);
    if (err != ESP_OK)
    {
        ESP_LOGE(TAG, "esp_wifi_set_config failed with 0x%X", err);
        wifi_state = WIFI_FAILED;
        wifi_disconnect();
        return err;
    }

    err = esp_wifi_start();
    if (err != ESP_OK)
    {
        ESP_LOGE(TAG, "esp_wifi_start failed with 0x%X", err);
        wifi_state = WIFI_FAILED;
        wifi_disconnect();
        return err;
    }

    ESP_LOGI(TAG, "Connecting to %s", (char *)wifi_config.sta.ssid);

    EventBits_t bits = xEventGroupWaitBits(s_wifi_event_group,
                                           WIFI_CONNECTED_BIT | WIFI_FAIL_BIT | WIFI_AUTHFAIL_BIT | WIFI_NO_AP_FOUND_BIT,
                                           pdFALSE,
                                           pdFALSE,
                                           WIFI_CONNECT_TIMEOUT / portTICK_PERIOD_MS);

    /* xEventGroupWaitBits() returns the bits before the call returned, hence we can test which event actually
     * happened. */

    led_stop_pattern(LED_CONNECTING);

    if (bits & WIFI_CONNECTED_BIT)
    {
        ESP_LOGI(TAG, "Connected to ap SSID:%s", (char *)wifi_config.sta.ssid);
        wifi_timeout_counter = 0;
        return ESP_OK;
    }
    else if (bits & WIFI_FAIL_BIT)
    {
        ESP_LOGI(TAG, "Failed to connect to SSID:%s", (char *)wifi_config.sta.ssid);
        wifi_disconnect();
        led_start_pattern(LED_CONNECTING_FAILED);
        return ESP_FAIL;
    }
    else if (bits & WIFI_AUTHFAIL_BIT)
    {
        ESP_LOGE(TAG, "Failed to connect to SSID:%s Auth fail", (char *)wifi_config.sta.ssid);
        wifi_timeout_counter++;
        wifi_state = WIFI_FAILED;
        led_start_pattern(LED_CONNECTING_FAILED);
        wifi_disconnect();
        return ESP_ERR_WIFI_SSID;
    }
    else if (bits & WIFI_NO_AP_FOUND_BIT)
    {
        ESP_LOGE(TAG, "Failed to connect to SSID:%s No AP found", (char *)wifi_config.sta.ssid);
        wifi_timeout_counter++;
        wifi_state = WIFI_FAILED;
        led_start_pattern(LED_CONNECTING_FAILED);
        wifi_disconnect();
        return ESP_ERR_WIFI_SSID;
    }
    else
    {
        ESP_LOGE(TAG, "Failed to connect to SSID:%s Timeout", (char *)wifi_config.sta.ssid);
        wifi_timeout_counter++;
        wifi_state = WIFI_FAILED;
        led_start_pattern(LED_CONNECTING_FAILED);
        wifi_disconnect();
        return ESP_FAIL;
    }
}

void wifi_disconnect()
{
    esp_err_t err = ESP_OK;
    if (wifi_state == WIFI_DISCONNECTED) // already disconnected
    {
        ESP_LOGD(TAG, "wifi already disconnected");
        return;
    }
    wifi_state = WIFI_DISCONNECTED;
    esp_wifi_set_ps(WIFI_PS_MAX_MODEM);
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
    ESP_LOGI(TAG, "Disconnected");
}

static void wifi_event_handler(void *arg, esp_event_base_t event_base, int32_t event_id, void *event_data)
{

    ESP_LOGI(TAG, "GOT EVENT: event_base: %s, event_id: %ld", event_base, event_id);
    if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_START && wifi_state != WIFI_DISCONNECTED)
    {
        esp_wifi_connect();
    }
    else if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_DISCONNECTED && wifi_state != WIFI_DISCONNECTED)
    {
        wifi_event_sta_disconnected_t *event = (wifi_event_sta_disconnected_t *)event_data;
        ESP_LOGI(TAG, "Disconnected from SSID:%s, reason:%u, rssi%d", (char *)event->ssid, event->reason, event->rssi);
        if (s_retry_num < ESP_MAXIMUM_RETRY)
        {
            wifi_state = WIFI_CONNECTING;
            esp_wifi_connect();
            s_retry_num++;
            ESP_LOGW(TAG, "Retry to connect to the AP: %d/%d", s_retry_num, ESP_MAXIMUM_RETRY);
        }
        else
        {
            switch (event->reason)
            {
            case WIFI_REASON_AUTH_FAIL:
            case WIFI_REASON_4WAY_HANDSHAKE_TIMEOUT:
                ESP_LOGE(TAG, "Auth fail");
                xEventGroupSetBits(s_wifi_event_group, WIFI_AUTHFAIL_BIT);
                break;
            case WIFI_REASON_NO_AP_FOUND:
                ESP_LOGE(TAG, "No AP found");
                xEventGroupSetBits(s_wifi_event_group, WIFI_NO_AP_FOUND_BIT);
                break;
            default:
                ESP_LOGE(TAG, "Connect to the AP fail");
                xEventGroupSetBits(s_wifi_event_group, WIFI_FAIL_BIT);
                break;
            }
            wifi_state = WIFI_FAILED;
            led_start_pattern(LED_CONNECTING_FAILED);
        }
    }
    else if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_CONNECTED)
    {
        ESP_LOGI(TAG, "Connected");
        wifi_state = WIFI_CONNECTED;
    }
    else if (event_base == IP_EVENT && event_id == IP_EVENT_STA_GOT_IP)
    {
        ip_event_got_ip_t *event = (ip_event_got_ip_t *)event_data;
        ESP_LOGI(TAG, "IP:" IPSTR, IP2STR(&event->ip_info.ip));
        wifi_current_ip = event->ip_info;
        s_retry_num = 0;
        wifi_state = WIFI_CONNECTED;
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
        // ESP_LOGE(TAG, "Unknown event: event_base: %s, event_id: %ld", event_base, event_id);
    }
}

static void wifi_time_sync_notification_cb(struct timeval *tv)
{
    // ESP_LOGW(TAG, "Notification of a time synchronization event");
}

time_t wifi_get_timestamp()
{
    static time_t now = 0;
    struct tm timeinfo;
    if (wifi_state == WIFI_CONNECTED)
    {
        ESP_LOGI(TAG, "Getting time over NTP");
        static bool sntp_started = false;
        static esp_sntp_config_t config = ESP_NETIF_SNTP_DEFAULT_CONFIG(NTP_SERVER);
        if (!sntp_started)
        {
            esp_sntp_setoperatingmode(SNTP_OPMODE_POLL);
            sntp_started = true;
            config.sync_cb = wifi_time_sync_notification_cb; // Note: This is only needed if we want
            sntp_set_sync_interval(0);                       // sync now
            esp_netif_sntp_init(&config);
            // uint8_t retry = 0;
            // while (sntp_get_sync_status() == SNTP_SYNC_STATUS_RESET && ++retry < 50)
            // {
            //     ESP_LOGI(TAG, "Waiting for system time to be set... (%d/%d)", retry, 50);
            //     vTaskDelay(1000 / portTICK_PERIOD_MS);
            // }
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
            err = esp_netif_sntp_sync_wait(1000 / portTICK_PERIOD_MS);
            // ESP_LOGI(TAG, "Waiting for system time to be set... (%d/%d) %d", retry, 3, err);
        } while (err == ESP_ERR_TIMEOUT && ++retry < 5);

        sntp_sync_status_t status = sntp_get_sync_status();
        if (status == SNTP_SYNC_STATUS_RESET)
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

static void wifi_init_softap(void)
{
    esp_err_t err;
    // Initialize Wi-Fi including netif with default config
    esp_netif_t *wifiAP = esp_netif_create_default_wifi_ap();
    esp_netif_ip_info_t ip_info;
    IP4_ADDR(&ip_info.ip, 4, 3, 2, 1);
    IP4_ADDR(&ip_info.gw, 4, 3, 2, 1);
    IP4_ADDR(&ip_info.netmask, 255, 255, 255, 0);
    esp_netif_dhcps_stop(wifiAP);
    esp_netif_set_ip_info(wifiAP, &ip_info);
    esp_netif_dhcps_start(wifiAP);

    // wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    // ESP_ERROR_CHECK(esp_wifi_init(&cfg));
    // ESP_ERROR_CHECK(esp_event_handler_register(WIFI_EVENT, ESP_EVENT_ANY_ID, &wifi_event_handler, NULL));

    wifi_config_t wifi_config = {};
    char ssid[32] = {0};
    // read MAC address
    uint8_t mac[6];
    esp_read_mac(mac, ESP_MAC_WIFI_SOFTAP);
    sprintf(ssid, "%s %s", AP_SSID, efuse_values.macAddress + 6);

    strncpy((char *)wifi_config.ap.ssid, ssid, sizeof(wifi_config.ap.ssid));
    strncpy((char *)wifi_config.ap.password, AP_PASS, sizeof(wifi_config.ap.password));
    wifi_config.ap.ssid_len = strlen(ssid);
    wifi_config.ap.authmode = WIFI_AUTH_WPA_WPA2_PSK;
    wifi_config.ap.max_connection = 4;
    if (strlen(AP_PASS) == 0)
    {
        wifi_config.ap.authmode = WIFI_AUTH_OPEN;
    }

    wifi_mode_t mode = WIFI_MODE_AP;
    if (wifi_state != WIFI_DISCONNECTED)
    {
        ESP_LOGI(TAG, "wifi_init_softap: APSTA mode");
        mode = WIFI_MODE_APSTA;
    }
    err = esp_wifi_set_mode(mode);
    if (err != ESP_OK)
    {
        ESP_LOGE(TAG, "esp_wifi_set_mode failed with 0x%X", err);
        return;
    }
    err = esp_wifi_set_config((wifi_interface_t)ESP_IF_WIFI_AP, &wifi_config);
    if (err != ESP_OK)
    {
        ESP_LOGE(TAG, "esp_wifi_set_config failed with 0x%X", err);
        return;
    }
    err = esp_wifi_start();
    if (err != ESP_OK)
    {
        ESP_LOGE(TAG, "esp_wifi_start failed with 0x%X", err);
        return;
    }
    ap_started = 1;

    // esp_netif_get_ip_info(esp_netif_get_handle_from_ifkey("WIFI_AP_DEF"), &ip_info);
    char ip_addr[16];
    inet_ntoa_r(ip_info.ip.addr, ip_addr, 16);
    ESP_LOGI(TAG, "Set up softAP with IP: %s", ip_addr);

    ESP_LOGI(TAG, "wifi_init_softap finished. SSID:'%s' password:'%s'",
             AP_SSID, AP_PASS);
}

static void stop_captive_portal_task(void *pvParameter)
{
    uint8_t readCount = 0;
    ap_started = 0;
    while (1)
    {
        if (!gpio_vusb_connected())
        {
            readCount++;
        }
        else
        {
            readCount = 0;
        }
        if (readCount > 6)
        {
            ESP_LOGI(TAG, "VUSB is not connected, stop captive portal");
            esp_restart();
        }
        // gpio_set_level(LED_GREEN, 1);
        // vTaskDelay(50 / portTICK_PERIOD_MS);
        // gpio_set_level(LED_GREEN, 0);
        vTaskDelay(500 / portTICK_PERIOD_MS);
    }
}

void wifi_start_captive_portal()
{
    ESP_LOGI(TAG, "Start captive portal");
    xTaskCreate(&stop_captive_portal_task, "stop_captive_portal_task", 2048, NULL, PRIORITY_STOP_CAPTIVE_PORTAL, NULL);

    // Initialise ESP32 in SoftAP mode
    wifi_init_softap();

    // Start the server for the first time
    setup_server();
    // Start the DNS server that will redirect all queries to the softAP IP
    start_dns_server();

    // wifi_config_t wifi_config = {
    //     .ap = {
    //         .ssid = "wifi1234",
    //         .ssid_len = 0,
    //         .max_connection = 4,
    //         .password = "12345678",
    //         .authmode = WIFI_AUTH_WPA_WPA2_PSK},
    // };

    // ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_AP));
    // ESP_ERROR_CHECK(esp_wifi_set_config(ESP_IF_WIFI_AP, &wifi_config));
    // ESP_ERROR_CHECK(esp_wifi_start());

    // wifi_connect();
}

void wifi_scan(uint16_t *ap_count)
{
    esp_err_t err;
    // err = esp_wifi_set_mode(WIFI_MODE_APSTA);
    // if (err != ESP_OK)
    // {
    //     ESP_LOGE(TAG, "esp_wifi_set_mode failed with 0x%X", err);
    //     return;
    // }
    if (wifi_state == WIFI_DISCONNECTED)
    {
        err = esp_wifi_start();

        if (err != ESP_OK)
        {
            ESP_LOGE(TAG, "esp_wifi_start failed with 0x%X", err);
            return;
        }
    }

    wifi_scan_config_t scan_config = {
        .ssid = 0,
        .bssid = 0,
        .channel = 0,
        .show_hidden = true};

    err = esp_wifi_scan_start(&scan_config, true);
    if (err != ESP_OK)
    {
        ESP_LOGE(TAG, "esp_wifi_scan_start failed with 0x%X", err);
        return;
    }
    ESP_LOGI(TAG, "Start scanning...");
    uint16_t ap_num = 0;
    esp_wifi_scan_get_ap_num(&ap_num);
    if (ap_count != NULL)
    {
        *ap_count = ap_num;
    }
    if (ap_num == 0)
    {
        ESP_LOGI(TAG, "No AP found");
        return;
    }
    err = esp_wifi_scan_get_ap_records(&ap_num, wifi_ap_list);
    if (err != ESP_OK)
    {
        ESP_LOGE(TAG, "esp_wifi_scan_get_ap_records failed with 0x%X", err);
        return;
    }
    for (int i = 0; i < ap_num; i++)
    {
        ESP_LOGI(TAG, "SSID: %s, RSSI: %d", wifi_ap_list[i].ssid, wifi_ap_list[i].rssi);
    }
}

static void ping_success(esp_ping_handle_t hdl, void *args)
{
    uint32_t time;
    esp_ping_get_profile(hdl, ESP_PING_PROF_TIMEGAP, &time, sizeof(time));

    if (ping_index < sizeof(ping_time_array) / sizeof(ping_time_array[0]))
    {
        ping_time_array[ping_index] = time;
        ping_index++;
    }

    ESP_LOGI(TAG, "Ping success: time=%ldms", time);
}

static void ping_timeout(esp_ping_handle_t hdl, void *args)
{
    ESP_LOGI(TAG, "Ping timeout");
}

static void ping_end(esp_ping_handle_t hdl, void *args)
{
    ESP_LOGI(TAG, "Ping end");

    uint32_t received;
    uint32_t transmitted;
    esp_ping_get_profile(hdl, ESP_PING_PROF_REQUEST, &transmitted, sizeof(transmitted));
    esp_ping_get_profile(hdl, ESP_PING_PROF_REPLY, &received, sizeof(received));
    ESP_LOGI(TAG, "Ping statistics: %ld packets transmitted, %ld received", transmitted, received);
    if (received == 0)
    {
        xEventGroupSetBits(ping_event_group, BIT1);
    }
    else
    {

        xEventGroupSetBits(ping_event_group, BIT0);
    }
}

esp_err_t wifi_ping(ip_addr_t host, uint32_t *ping_time)
{
    ping_index = 0;
    for (int i = 0; i < sizeof(ping_time_array) / sizeof(ping_time_array[0]); i++)
    {
        ping_time_array[i] = UINT32_MAX;
    }

    esp_ping_config_t ping_config = ESP_PING_DEFAULT_CONFIG();
    ping_config.target_addr = host;
    ping_config.count = 4;
    esp_ping_handle_t ping;

    esp_ping_callbacks_t cbs = {
        .on_ping_success = ping_success,
        .on_ping_timeout = ping_timeout,
        .on_ping_end = ping_end,
    };

    esp_err_t err = esp_ping_new_session(&ping_config, &cbs, &ping);
    if (err != ESP_OK)
    {
        ESP_LOGE(TAG, "Failed to start ping session: %d", err);
        return err;
    }
    err = esp_ping_start(ping);

    EventBits_t bits = xEventGroupWaitBits(ping_event_group,
                                           BIT0 | BIT1,
                                           pdFALSE,
                                           pdFALSE,
                                           5000 / portTICK_PERIOD_MS);

    esp_ping_delete_session(ping);

    uint32_t sum = 0;
    uint32_t count = 0;
    for (int i = 0; i < sizeof(ping_time_array) / sizeof(ping_time_array[0]); i++)
    {
        if (ping_time_array[i] == UINT32_MAX)
        {
            continue;
        }
        sum += ping_time_array[i];
        count++;
    }
    if (ping_time != NULL)
    {
        if (count > 0)
        {
            *ping_time = sum / count;
        }
        else
        {
            *ping_time = UINT32_MAX;
        }
    }

    if (bits & BIT0)
    {
        ESP_LOGI(TAG, "Ping success");
        return ESP_OK;
    }
    else if (bits & BIT1)
    {
        ESP_LOGI(TAG, "Ping failed");
        return ESP_FAIL;
    }
    else
    {
        ESP_LOGI(TAG, "Ping timeout");
        return ESP_ERR_TIMEOUT;
    }
}