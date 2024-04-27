/**
 * @file main.cpp
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
#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/event_groups.h"
#include "driver/gpio.h"
#include "driver/uart.h"

#include "freertos/timers.h"
#include "esp_timer.h"
#include "esp_wifi.h"
#include "nvs_flash.h"
#include "esp_netif.h"
#include "esp_http_client.h"
#include "esp_log.h"
#include "esp_sleep.h"
#include "cJSON.h"
#include "string.h"
#include "sdkconfig.h"
#include "esp_ota_ops.h"
#include "esp_pm.h"
#include "esp_private/esp_clk.h"

#include "common.h"
#include "linky.h"
#include "main.h"
#include "config.h"
#include "wifi.h"
#include "shell.h"
#include "mqtt.h"
#include "gpio.h"
#include "web.h"
#include "zigbee.h"
#include "tuya.h"
#include "ota.h"
#include "power.h"
#include "led.h"
#include "tests.h"

#include "esp_heap_trace.h"
#include "esp_err.h"

/*==============================================================================
 Local Define
===============================================================================*/
#define MAIN_TAG "MAIN"

/*==============================================================================
 Local Macro
===============================================================================*/

/*==============================================================================
 Local Type
===============================================================================*/

/*==============================================================================
 Local Function Declaration
===============================================================================*/
static void main_print_heap_diff();
/*==============================================================================
Public Variable
===============================================================================*/
TaskHandle_t main_task_handle = NULL;
uint32_t main_sleep_time = 99999;
/*==============================================================================
 Local Variable
===============================================================================*/
static esp_pm_lock_handle_t main_init_lock;

#define MAX_DATA_INDEX 5
linky_data_t main_data_array[MAX_DATA_INDEX];
unsigned int main_data_index = 0;
uint64_t main_next_update_check;

/*==============================================================================
Function Implementation
===============================================================================*/
void app_main(void)
{
  esp_err_t err;

  ESP_LOGI(MAIN_TAG, "Starting TICMeter...");
  power_init();
  shell_wake_reason();
  gpio_init_pins();
  config_begin();
  led_start_pattern(LED_BOOT);
  linky_init(MODE_HIST, RX_LINKY);
  esp_pm_lock_create(ESP_PM_APB_FREQ_MAX, 0, "main_init", &main_init_lock);
  esp_pm_lock_acquire(main_init_lock);

  if (config_values.mode != MODE_ZIGBEE) // TODO: check why in zigbee mode, the wifi_init is not working
  {
    shell_init();
    wifi_init();
  }
  else
  {
    if (config_values.zigbee.state == ZIGBEE_WANT_PAIRING)
    {
      ESP_LOGI(MAIN_TAG, "Zigbee pairing mode");
      led_start_pattern(LED_PAIRING);
    }
  }

  // ESP_LOGI(MAIN_TAG, "VICTOIREEEEEEEErEEEEEEEEEEEEEEEEEE123456789");

  // if (gpio_get_vcondo() < 3.4)
  // {
  //   ESP_LOGW(MAIN_TAG, "VCondo too low, light sleep for 30s");
  //   esp_pm_lock_release(main_init_lock);
  //   vTaskDelay(30000 / portTICK_PERIOD_MS);
  //   esp_pm_lock_acquire(main_init_lock);
  // }

  // linky_want_debug_frame = 4;

  // start_test(TEST_LINKY_STD);
  // esp_pm_dump_locks(stdout);

  if (config_verify() || config_values.boot_pairing)
  {
    // esp_pm_lock_release(main_init_lock);
    if (config_values.boot_pairing)
    {
      ESP_LOGI(MAIN_TAG, "Waking up in pairing mode. Waiting config...");
      ESP_LOGI(MAIN_TAG, "Remove pairing mode for next boot");
      config_values.boot_pairing = 0;
      config_write();
      gpio_start_pariring();
    }
    else
    {
      ESP_LOGW(MAIN_TAG, "No config found. Waiting for config...");
    }

    while (1)
    {
      if (gpio_start_push_time + 5000 < MILLIS && !config_values.boot_pairing) // want 5s after button push
      {
        led_start_pattern(LED_NO_CONFIG);
      }
      vTaskDelay(5000 / portTICK_PERIOD_MS);
    }
    // esp_pm_lock_acquire(main_init_lock);
  }
  ESP_LOGI(MAIN_TAG, "Config found. Starting...");

  vTaskDelay(200 / portTICK_PERIOD_MS); // for led pattern

  if (config_values.mode == MODE_ZIGBEE && !linky_update())
  {
    while (!linky_update())
    {
      ESP_LOGE(MAIN_TAG, "Cant find Linky: retrying every 10s before starting");
      vTaskDelay(10000 / portTICK_PERIOD_MS);
    }
  }

  ESP_LOGI(MAIN_TAG, "Linky found");
  switch (config_values.mode)
  {
  case MODE_WEB:
    // connect to wifi
    err = wifi_connect();
    if (err == ESP_OK)
    {
      wifi_get_timestamp();               // get timestamp from ntp server
      wifi_http_get_config_from_server(); // get config from server
      vTaskDelay(1000 / portTICK_PERIOD_MS);
      if (gpio_vusb_connected())
      {
        ota_version_t version;
        ota_get_latest(&version);
      }
      wifi_disconnect();
    }
    else
    {
      ESP_LOGE(MAIN_TAG, "Wifi connection failed: dont start HTTP");
    }
    break;
  case MODE_MQTT:
  case MODE_MQTT_HA:
    ESP_LOGI(MAIN_TAG, "MQTT init...");
    // connect to wifi
    err = wifi_connect();
    if (err == ESP_OK)
    {
      mqtt_init();          // init mqtt
      wifi_get_timestamp(); // get timestamp from ntp server
      if (gpio_vusb_connected())
      {
        ota_version_t version;
        ota_get_latest(&version);
      }
      vTaskDelay(1000 / portTICK_PERIOD_MS);
      wifi_disconnect();
    }
    else
    {
      ESP_LOGE(MAIN_TAG, "Wifi connection failed: dont start MQTT");
    }

    break;
  case MODE_ZIGBEE:
    power_set_zigbee();
    zigbee_init_stack();
    vTaskDelay(2000 / portTICK_PERIOD_MS);
    break;
  case MODE_TUYA:
    if (config_values.pairing_state != TUYA_PAIRED)
    {
      ESP_LOGW(MAIN_TAG, "Tuya not paired.");
      break;
    }
    err = wifi_connect();
    if (err == ESP_OK)
    {
      tuya_init();
      tuya_state = true;
      vTaskDelay(1000 / portTICK_PERIOD_MS);

      if (!gpio_vusb_connected())
      {
        wifi_disconnect();
        vTaskSuspend(tuyaTaskHandle);
        tuya_state = false;
      }
    }
    else
    {
      ESP_LOGE(MAIN_TAG, "Wifi connection failed: dont start TUYA");
    }
    break;
  default:
    break;
  }
  // start linky fetch task
  xTaskCreate(main_task, "main_task_handle", 16 * 1024, NULL, PRIORITY_FETCH_LINKY, &main_task_handle); // start linky task
  esp_pm_lock_release(main_init_lock);
}
void main_task(void *pvParameters)
{
  ESP_LOGI(MAIN_TAG, "Starting fetch linky data task");
  main_next_update_check = MILLIS;
  esp_err_t err;
  uint32_t err_count = 0;
  int32_t fetching_time = 0;
  switch (config_values.mode)
  {
  case MODE_WEB:
    fetching_time = 10;
    break;
  case MODE_MQTT:
  case MODE_MQTT_HA:
    fetching_time = 10;
    break;
  case MODE_ZIGBEE:
    fetching_time = 2;
    break;
  case MODE_TUYA:
    fetching_time = 10;
    break;
  default:
    fetching_time = 10;
    break;
  }

  while (1)
  {
    main_sleep_time = abs(config_values.refreshRate - fetching_time);
    // esp_pm_dump_locks(stdout);
    ESP_LOGI(MAIN_TAG, "Waiting for %ld seconds", main_sleep_time);
    esp_pm_lock_release(main_init_lock);
    while (main_sleep_time > 0)
    {
      vTaskDelay(1000 / portTICK_PERIOD_MS);
      main_sleep_time--;
    }

    esp_pm_lock_acquire(main_init_lock);
    gpio_peripheral_reinit();
    ESP_LOGI(MAIN_TAG, "-----------------------------------------------------------------");
    ESP_LOGI(MAIN_TAG, "Waking up, VCondo: %f", gpio_get_vcondo());

    if (!linky_update() || !linky_presence())
    {
      ESP_LOGE(MAIN_TAG, "Linky update failed");
      led_start_pattern(LED_LINKY_FAILED);
      continue;
    }

    err = main_send_data();
    if (err != ESP_OK)
    {
      err_count++;
      ESP_LOGE(MAIN_TAG, "Data send failed %ld times", err_count);
      if (err_count > 10)
      {
        ESP_LOGE(MAIN_TAG, "Too many errors, rebooting");
        hard_restart();
      }
    }
    else
    {
      err_count = 0;
    }
    main_print_heap_diff();
  }
}

esp_err_t main_send_data()
{
  esp_err_t err;
  linky_uptime = esp_timer_get_time() / 1000000;

  switch (config_values.mode)
  {
  case MODE_WEB:
  {
    // send data to web server
    if (main_data_index >= MAX_DATA_INDEX)
    {
      main_data_index = 0;
    }
    main_data_array[main_data_index] = linky_data;
    main_data_array[main_data_index++].timestamp = wifi_get_timestamp();
    ESP_LOGI(MAIN_TAG, "Data stored: %d - BASE: %lld", main_data_index, main_data_array[0].timestamp);
    if (1)
    {
      char json[1024] = {0};
      web_preapare_json_data(main_data_array, main_data_index, json, sizeof(json));
      ESP_LOGI(MAIN_TAG, "Sending data to server");
      err = wifi_connect();
      if (err == ESP_OK)
      {
        ESP_LOGI(MAIN_TAG, "POST: %s", json);
        wifi_send_to_server(json);
        if (gpio_vusb_connected())
        {
          ota_version_t version;
          ota_get_latest(&version);
        }
      }
      else
      {
        ESP_LOGE(MAIN_TAG, "Wifi connection failed");
        return ESP_FAIL;
      }
      wifi_disconnect();
      main_data_index = 0;
      return ESP_OK;
    }
    break;
  }
  case MODE_MQTT:
  case MODE_MQTT_HA: // send data to mqtt server
  {
    linky_free_heap_size = esp_get_free_heap_size();
    uint8_t ret = mqtt_prepare_publish(&linky_data);
    if (ret == 0)
    {
      ESP_LOGE(MAIN_TAG, "Some data will not be sent, but we continue");
    }
    ret = wifi_connect();
    if (ret != ESP_OK)
    {
      ESP_LOGE(MAIN_TAG, "Wifi connection failed");
      goto send_error;
    }
    ESP_LOGI(MAIN_TAG, "Sending data to MQTT");
    ret = mqtt_send();
    if (ret == 0)
    {
      ESP_LOGE(MAIN_TAG, "MQTT send failed");
      goto send_error;
    }

    if (gpio_vusb_connected() && main_next_update_check < MILLIS)
    {
      ESP_LOGI(MAIN_TAG, "Checking for update");
      main_next_update_check = MILLIS + 3600000;
      ota_version_t version;
      ota_get_latest(&version);
    }
    wifi_disconnect();
    led_start_pattern(LED_SEND_OK);
    return ESP_OK;
    break;

  send_error:
    wifi_disconnect();
    led_start_pattern(LED_SEND_FAILED);
    return ESP_FAIL;
    break;
  }
  case MODE_TUYA:
    err = wifi_connect();
    if (err == ESP_OK)
    {
      ESP_LOGI(MAIN_TAG, "Sending data to TUYA");
      if (tuya_state == false)
      {
        ESP_LOGE(MAIN_TAG, "Tuya not connected, reconnecting...");
        resumeTask(tuyaTaskHandle); // resume tuya task
        tuya_state = true;
        if (tuya_wait_event(TUYA_EVENT_MQTT_CONNECTED, 10000))
        {
          ESP_LOGE(MAIN_TAG, "Tuya MQTT ERROR");
          led_start_pattern(LED_SEND_FAILED);
          err = ESP_FAIL;
          goto tuya_disconect;
        }
      }

      if (tuya_send_data(&linky_data))
      {
        ESP_LOGE(MAIN_TAG, "Tuya SEND ERROR");
        led_start_pattern(LED_SEND_FAILED);
        err = ESP_FAIL;
        goto tuya_disconect;
      }
      err = ESP_OK;
    tuya_disconect:
      if (gpio_vusb_connected() && main_next_update_check < MILLIS)
      {
        ESP_LOGI(MAIN_TAG, "Checking for update");
        main_next_update_check = MILLIS + 3600000;
        ota_version_t version;
        ota_get_latest(&version);
      }

      if (!gpio_vusb_connected())
      {
        ESP_LOGI(MAIN_TAG, "VUSB not connected, suspend TUYA");
        wifi_disconnect();
        suspendTask(tuyaTaskHandle);
        tuya_state = false;
      }
      led_start_pattern(LED_SEND_OK);
      return err;
    }
    else
    {
      ESP_LOGE(MAIN_TAG, "Wifi connection failed: dont send TUYA");
      return ESP_FAIL;
    }
    break;
  case MODE_ZIGBEE:
    zigbee_send(&linky_data);
    break;
  default:
    break;
  }
  return ESP_OK;
}

static void main_print_heap_diff()
{
  static uint32_t last_heap = 0;
  ESP_LOGI(MAIN_TAG, "Free heap memory: %ld", esp_get_free_heap_size());
  int32_t diff = last_heap - esp_get_free_heap_size();
  if (diff > 0)
  {
    ESP_LOGW(MAIN_TAG, "Heap: we lost %ld bytes", diff);
  }
  else
  {
    ESP_LOGI(MAIN_TAG, "Heap: we gained %ld bytes", -diff);
  }
  last_heap = esp_get_free_heap_size();
}
