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
#include "esp_system.h"

/*==============================================================================
 Local Define
===============================================================================*/
#define MAIN_TAG "MAIN"

#define OTA_CHECK_TIME 4 * 3600 * 1000 // 4 hours
#define MAIN_BOOT_VOLTAGE_THRESHOLD 4.0

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
static void main_ota_check();

/*==============================================================================
Public Variable
===============================================================================*/
TaskHandle_t main_task_handle = NULL;
uint32_t main_sleep_time = 99999;
/*==============================================================================
 Local Variable
===============================================================================*/
static esp_pm_lock_handle_t main_init_lock;

linky_data_t main_data_array[MAX_DATA_INDEX];
unsigned int main_data_index = 0;

/*==============================================================================
Function Implementation
===============================================================================*/
void app_main(void)
{
  esp_err_t err;

  ESP_LOGI(MAIN_TAG, "Starting TICMeter...");
  power_init();
  if (shell_wake_reason() == ESP_RST_BROWNOUT)
  {
    ESP_LOGE(MAIN_TAG, "Brownout detected sleeping for 30s...");
    vTaskDelay(30000 / portTICK_PERIOD_MS);
  }

  gpio_init_pins();

  while (!gpio_vusb_connected() && gpio_get_vcondo() < MAIN_BOOT_VOLTAGE_THRESHOLD)
  {
    led_start_pattern(LED_CHARGING);
    ESP_LOGW(MAIN_TAG, "Waiting for capacitor to charge: %fV / %fV: waiting 10s", gpio_get_vcondo(), MAIN_BOOT_VOLTAGE_THRESHOLD);
    vTaskDelay(10000 / portTICK_PERIOD_MS);
  }
  led_start_pattern(LED_CHARGING);
  config_begin();
  led_start_pattern(LED_BOOT);
  linky_init(RX_LINKY);
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

  // if (gpio_get_vcondo() < 3.4)
  // {
  //   ESP_LOGW(MAIN_TAG, "VCondo too low, light sleep for 30s");
  //   esp_pm_lock_release(main_init_lock);
  //   vTaskDelay(30000 / portTICK_PERIOD_MS);
  //   esp_pm_lock_acquire(main_init_lock);
  // }

  // linky_debug = DEBUG_STD;

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
      vTaskDelay(portMAX_DELAY);
    }
    else
    {
      ESP_LOGW(MAIN_TAG, "No config found. Waiting for config...");
    }

    esp_pm_lock_release(main_init_lock);
    while (1)
    {
      if (gpio_start_push_time + 5000 < MILLIS && !config_values.boot_pairing) // want 5s after button push
      {
        led_start_pattern(LED_NO_CONFIG);
      }
      vTaskDelay(5000 / portTICK_PERIOD_MS);
    }
  }
  ESP_LOGI(MAIN_TAG, "Config found. Starting...");

  vTaskDelay(200 / portTICK_PERIOD_MS); // for led pattern

  if (config_values.mode == MODE_ZIGBEE && !linky_update(LINKY_READING_TIMEOUT))
  {
    while (!linky_update(LINKY_READING_TIMEOUT))
    {
      ESP_LOGE(MAIN_TAG, "Cant find Linky: retrying every 10s before starting");
      esp_pm_lock_release(main_init_lock);
      vTaskDelay(10000 / portTICK_PERIOD_MS);
      esp_pm_lock_acquire(main_init_lock);
    }
  }

  ESP_LOGI(MAIN_TAG, "Linky found");
  switch (config_values.mode)
  {
  case MODE_HTTP:
    // connect to wifi
    err = wifi_connect();
    if (err == ESP_OK)
    {
      wifi_get_timestamp();               // get timestamp from ntp server
      wifi_http_get_config_from_server(); // get config from server
      vTaskDelay(1000 / portTICK_PERIOD_MS);
      main_ota_check();
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
      main_ota_check();
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
  esp_err_t err;
  uint32_t err_count = 0;
  const uint32_t fetching_time[] = {
      [MODE_HTTP] = 10,
      [MODE_MQTT] = 10,
      [MODE_MQTT_HA] = 10,
      [MODE_ZIGBEE] = 2,
      [MODE_TUYA] = 10,
  };
  linky_clear_data();

  while (1)
  {
    main_sleep_time = abs((int32_t)config_values.refresh_rate - (int32_t)fetching_time[config_values.mode] - ((LINKY_READING_TIMEOUT / 1000) - 2));
    ESP_LOGI(MAIN_TAG, "Waiting for %ld seconds", main_sleep_time);
    esp_pm_lock_release(main_init_lock);
    while (main_sleep_time > 0)
    {
      vTaskDelay(1000 / portTICK_PERIOD_MS);
      main_sleep_time--;
    }

    gpio_peripheral_reinit();
    ESP_LOGI(MAIN_TAG, "-----------------------------------------------------------------");
    ESP_LOGI(MAIN_TAG, "Waking up, VCondo: %f", gpio_get_vcondo());

    if (!linky_update(LINKY_READING_TIMEOUT) /* || !linky_presence()*/)
    {
      ESP_LOGE(MAIN_TAG, "Linky update failed");
      led_start_pattern(LED_LINKY_FAILED);
      continue;
    }
    esp_pm_lock_acquire(main_init_lock);
    linky_print();
    linky_stats();

    // esp_pm_dump_locks(stdout);
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
  esp_err_t err = ESP_OK;
  switch (config_values.mode)
  {
  case MODE_HTTP:
  {
    // send data to web server
    if (main_data_index >= MAX_DATA_INDEX)
    {
      main_data_index = 0;
    }
    main_data_array[main_data_index++] = linky_data;
    ESP_LOGI(MAIN_TAG, "Data stored: %d/%d: time: %lld", main_data_index, config_values.web.store_before_send, linky_data.timestamp);
    if (main_data_index >= config_values.web.store_before_send || main_data_index >= MAX_DATA_INDEX)
    {
      char *json = NULL;
      web_preapare_json_data(main_data_array, main_data_index, &json);
      if (json == NULL)
      {
        ESP_LOGE(MAIN_TAG, "Cant prepare json data");
        err = ESP_FAIL;
        break;
      }

      ESP_LOGI(MAIN_TAG, "Sending data to server");
      err = wifi_connect();
      if (err == ESP_OK)
      {
        ESP_LOGI(MAIN_TAG, "POST: %s", json);
        wifi_send_to_server(json);
        main_ota_check();
        err = ESP_OK;
      }
      else
      {
        ESP_LOGE(MAIN_TAG, "Wifi connection failed");
        err = ESP_FAIL;
      }
      free(json);
      wifi_disconnect();
      main_data_index = 0;
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

    main_ota_check();
    vTaskDelay(100 / portTICK_PERIOD_MS);
    wifi_disconnect();
    led_start_pattern(LED_SEND_OK);
    break;

  send_error:
    wifi_disconnect();
    led_start_pattern(LED_SEND_FAILED);
    err = ESP_FAIL;
    break;
  }
  case MODE_TUYA:
    if (config_values.index_offset.value_saved == 0)
    {
      ESP_LOGI(MAIN_TAG, "Index offset not saved, reread Linky to be sure...");
      linky_update(LINKY_READING_TIMEOUT);
      tuya_fill_index(&config_values.index_offset, &linky_data);
      config_values.index_offset.value_saved = 1;
      config_write();
    }

    err = wifi_connect();
    if (err == ESP_OK)
    {
      ESP_LOGI(MAIN_TAG, "Sending data to TUYA");
      if (tuya_state == false)
      {
        ESP_LOGW(MAIN_TAG, "Tuya not connected, reconnecting...");
        resume_task(tuyaTaskHandle); // resume tuya task
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
      main_ota_check();
      if (!gpio_vusb_connected())
      {
        ESP_LOGI(MAIN_TAG, "VUSB not connected, suspend TUYA");
        wifi_disconnect();
        suspend_task(tuyaTaskHandle);
        tuya_state = false;
      }
      led_start_pattern(LED_SEND_OK);
    }
    else
    {
      ESP_LOGE(MAIN_TAG, "Wifi connection failed: dont send TUYA");
      err = ESP_FAIL;
    }
    break;
  case MODE_ZIGBEE:
    zigbee_send(&linky_data);
    break;
  default:
    break;
  }
  ESP_LOGI(MAIN_TAG, "Data sent, clear data");
  linky_clear_data();
  return err;
}

static void main_ota_check()
{
  static uint64_t next_update_check = 0;
  if (next_update_check == 0)
  {
    // first check in 2 minutes
    next_update_check = MILLIS + 2 * 60 * 1000; // 2 minutes
  }

  if (next_update_check < MILLIS)
  {
    ota_version_t version;
    ESP_LOGI(MAIN_TAG, "Checking for update");
    next_update_check = MILLIS + OTA_CHECK_TIME;
    ota_get_latest(&version);
  }
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
