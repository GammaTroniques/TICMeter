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
/*==============================================================================
 Local Define
===============================================================================*/
#define TAG "WIFI"
#define NTP_SERVER "pool.ntp.org"

#define ESP_MAXIMUM_RETRY 4
#define WIFI_CONNECT_FAIL_COUNT_BEFORE_RESET 10
#define WIFI_CONNECTED_BIT BIT0
#define WIFI_FAIL_BIT BIT1
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
uint8_t wifi_sending = 0;
uint32_t wifi_timeout_counter = 0;

/*==============================================================================
 Local Variable
===============================================================================*/
static int s_retry_num = 0;
static EventGroupHandle_t s_wifi_event_group;

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
    return 1;
}

uint8_t wifi_connect()
{
    esp_err_t err = ESP_OK;
    if (wifi_state == WIFI_CONNECTED) // already connected
        return 1;

    if (strlen(config_values.ssid) == 0 || strlen(config_values.password) == 0)
    {
        ESP_LOGI(TAG, "No Wifi SSID or password");
        return 0;
    }
    if (wifi_timeout_counter > WIFI_CONNECT_FAIL_COUNT_BEFORE_RESET)
    {
        ESP_LOGE(TAG, "Too many wifi timeout (%ld): Hard reset", wifi_timeout_counter);
        gpio_set_direction(GPIO_NUM_15, GPIO_MODE_OUTPUT);
        gpio_set_level(GPIO_NUM_15, 0);
        return 0;
    }

    wifi_state = WIFI_CONNECTING;
    // xTaskCreate(gpio_led_task_wifi_connecting, "gpio_led_task_wifi_connecting", 4096, NULL, 1, NULL); // start wifi connect led task

    s_retry_num = 0;
    xEventGroupClearBits(s_wifi_event_group, WIFI_CONNECTED_BIT | WIFI_FAIL_BIT);

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
    err = esp_wifi_set_mode(WIFI_MODE_STA);
    if (err != ESP_OK)
    {
        ESP_LOGE(TAG, "esp_wifi_set_mode failed with 0x%X", err);
        wifi_state = WIFI_FAILED;
        wifi_disconnect();
        return 0;
    }
    err = esp_wifi_set_config(WIFI_IF_STA, &wifi_config);
    if (err != ESP_OK)
    {
        ESP_LOGE(TAG, "esp_wifi_set_config failed with 0x%X", err);
        wifi_state = WIFI_FAILED;
        wifi_disconnect();
        return 0;
    }

    err = esp_wifi_start();
    if (err != ESP_OK)
    {
        ESP_LOGE(TAG, "esp_wifi_start failed with 0x%X", err);
        wifi_state = WIFI_FAILED;
        wifi_disconnect();
        return 0;
    }

    ESP_LOGI(TAG, "Connecting to %s", (char *)wifi_config.sta.ssid);

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
        wifi_timeout_counter = 0;
        return 1;
    }
    else if (bits & WIFI_FAIL_BIT)
    {
        ESP_LOGI(TAG, "Failed to connect to SSID:%s", (char *)wifi_config.sta.ssid);
        wifi_disconnect();
        return 0;
    }
    else
    {
        ESP_LOGE(TAG, "Failed to connect to SSID:%s Timeout", (char *)wifi_config.sta.ssid);
        wifi_timeout_counter++;
        wifi_state = WIFI_FAILED;
        // gpio_start_led_pattern(PATTERN_WIFI_FAILED);
        wifi_disconnect();
        return 0;
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

    // ESP_LOGI(TAG, "GOT EVENT: event_base: %s, event_id: %ld", event_base, event_id);
    if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_START && wifi_state != WIFI_DISCONNECTED)
    {
        esp_wifi_connect();
    }
    else if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_DISCONNECTED && wifi_state != WIFI_DISCONNECTED)
    {
        if (s_retry_num < ESP_MAXIMUM_RETRY)
        {
            wifi_state = WIFI_CONNECTING;
            esp_wifi_connect();
            s_retry_num++;
            ESP_LOGW(TAG, "Retry to connect to the AP: %d/%d", s_retry_num, ESP_MAXIMUM_RETRY);
        }
        else
        {
            xEventGroupSetBits(s_wifi_event_group, WIFI_FAIL_BIT);
            ESP_LOGE(TAG, "Connect to the AP fail");
            gpio_start_led_pattern(PATTERN_WIFI_FAILED);
            wifi_state = WIFI_FAILED;
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

    ESP_ERROR_CHECK(esp_event_handler_register(WIFI_EVENT, ESP_EVENT_ANY_ID, &wifi_event_handler, NULL));

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

static void stop_captive_portal_task(void *pvParameter)
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
    xTaskCreate(&stop_captive_portal_task, "stop_captive_portal_task", 2048, NULL, 1, NULL);
    // // Initialize networking stack
    // ESP_ERROR_CHECK(esp_netif_init());

    // // Create default event loop needed by the  main app
    // ESP_ERROR_CHECK(esp_event_loop_create_default());

    // // Initialize NVS needed by Wi-Fi
    // ESP_ERROR_CHECK(nvs_flash_init());

    // Initialise ESP32 in SoftAP mode
    wifi_init_softap();

    // Start the server for the first time
    setup_server();
    // Start the DNS server that will redirect all queries to the softAP IP
    start_dns_server();
}
