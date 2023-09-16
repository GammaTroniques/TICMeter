#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/event_groups.h"
#include "driver/gpio.h"
#include "driver/uart.h"

#include "freertos/timers.h"
#include "esp_wifi.h"
#include "nvs_flash.h"
#include "esp_netif.h"
#include "esp_http_client.h"
#include "esp_log.h"
#include "esp_sleep.h"
#include "cJSON.h"

#include "sdkconfig.h"

#include "string.h"
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

Config config;
TaskHandle_t fetchLinkyDataTaskHandle = NULL;

#define MAIN_TAG "MAIN"
extern "C" void app_main(void)
{
  ESP_LOGI(MAIN_TAG, "Starting ESP32 Linky...");
  initPins();
  startLedPattern(PATTERN_START);
  xTaskCreate(pairingButtonTask, "pairingButtonTask", 8192, NULL, 1, NULL); // start push button task
  config.begin();
  shellInit(); // init shell

  if (config.verify())
  {
    xTaskCreate(noConfigLedTask, "noConfigLedTask", 1024, NULL, 1, NULL); // start no config led task
    ESP_LOGI(MAIN_TAG, "No config found. Waiting for config...");
    while (config.verify())
    {
      vTaskDelay(1000 / portTICK_PERIOD_MS);
    }
  }

  // check if VCondo is too low and go to deep sleep
  // the BOOT_PIN is used to prevent deep sleep when the device is plugged to a computer for debug
  if (getVUSB() < 3 && getVCondo() < 3.5 && config.values.enableDeepSleep && gpio_get_level(BOOT_PIN))
  {
    ESP_LOGI(MAIN_TAG, "VCondo is too low, going to deep sleep");
    esp_sleep_enable_timer_wakeup(10 * 1000000); // 10 second
    esp_deep_sleep_start();
  }

  switch (config.values.mode)
  {
  case MODE_WEB:
    // connect to wifi
    if (connectToWifi())
    {
      getTimestamp();               // get timestamp from ntp server
      getConfigFromServer(&config); // get config from server
      vTaskDelay(1000 / portTICK_PERIOD_MS);
      disconectFromWifi();
    }
    break;
  case MODE_MQTT:
  case MODE_MQTT_HA:
    // connect to wifi
    if (connectToWifi())
    {
      getTimestamp(); // get timestamp from ntp server
      vTaskDelay(1000 / portTICK_PERIOD_MS);
      disconectFromWifi();
    }
    break;
  case MODE_ZIGBEE:
    // init_zigbee();
    // zigbee_task(0);
    break;
  case MODE_TUYA:
    if (connectToWifi())
    {
      init_tuya();
      vTaskDelay(1000 / portTICK_PERIOD_MS);
      vTaskSuspend(tuyaTaskHandle);
      disconectFromWifi();
    }
    break;
  default:
    break;
  }
  // start linky fetch task

  xTaskCreate(fetchLinkyDataTask, "fetchLinkyDataTask", 16384, NULL, 1, &fetchLinkyDataTaskHandle); // start linky task
}

void fetchLinkyDataTask(void *pvParameters)
{
#define MAX_DATA_INDEX 5
  LinkyData dataArray[MAX_DATA_INDEX];
  unsigned int dataIndex = 0;
  linky.begin();
  while (1)
  {
    if (!linky.update() ||
        !linky.presence())
    {
      ESP_LOGE(MAIN_TAG, "Linky update failed: \n %s", linky.buffer);
      startLedPattern(PATTERN_LINKY_ERR);
      vTaskDelay((config.values.refreshRate * 1000) / portTICK_PERIOD_MS); // wait for refreshRate seconds before next loop
      continue;
    }
    linky.print();
    switch (config.values.mode)
    {
    case MODE_WEB: // send data to web server
      if (dataIndex >= MAX_DATA_INDEX)
      {
        dataIndex = 0;
      }
      dataArray[dataIndex] = linky.data;
      dataArray[dataIndex++].timestamp = getTimestamp();
      ESP_LOGI(MAIN_TAG, "Data stored: %d - BASE: %lld", dataIndex, dataArray[0].timestamp);
      if (dataIndex > 2)
      {
        char json[1024] = {0};
        preapareJsonData(dataArray, dataIndex, json, sizeof(json));
        ESP_LOGI(MAIN_TAG, "Sending data to server");
        if (connectToWifi())
        {
          ESP_LOGI(MAIN_TAG, "POST: %s", json);
          sendToServer(json);
        }
        disconectFromWifi();
        dataIndex = 0;
      }
      break;
    case MODE_MQTT:
    case MODE_MQTT_HA: // send data to mqtt server
      if (connectToWifi())
      {
        ESP_LOGI(MAIN_TAG, "Sending data to MQTT");
        sendToMqtt(&linky.data);
        disconectFromWifi();
        startLedPattern(PATTERN_SEND_OK);
      }
      else
      {
        startLedPattern(PATTERN_SEND_ERR);
      }
      break;
    case MODE_TUYA:
      if (connectToWifi())
      {
        ESP_LOGI(MAIN_TAG, "Sending data to TUYA");
        vTaskResume(tuyaTaskHandle);
        send_tuya_data(&linky.data);
        vTaskDelay(5000 / portTICK_PERIOD_MS);
        vTaskSuspend(tuyaTaskHandle);
        disconectFromWifi();
        startLedPattern(PATTERN_SEND_OK);
      }
      break;
    case MODE_ZIGBEE:
      sendToZigbee(&linky.data);
    default:
      break;
    }

    uint32_t sleepTime = abs(config.values.refreshRate - 5);
    if (config.values.enableDeepSleep && getVUSB() < 3) // if deepsleep is enable and we are not connected to USB
    {
      ESP_LOGI(MAIN_TAG, "Going to sleep for %ld seconds", sleepTime);
      esp_sleep_enable_timer_wakeup(sleepTime * 1000000); // wait for refreshRate seconds before next loop
      esp_light_sleep_start();
    }
    else
    {
      ESP_LOGI(MAIN_TAG, "PAPP: %d", (uint16_t) * &linky.data.hist.PAPP);
      ESP_LOGI(MAIN_TAG, "Waiting for %ld seconds", sleepTime);

      while (sleepTime > 0)
      {
        vTaskDelay(1000 / portTICK_PERIOD_MS);
        sleepTime--;
        if (getVUSB() < 3)
        {
          ESP_LOGI(MAIN_TAG, "USB disconnected, going to sleep for %ld seconds", sleepTime);
          esp_sleep_enable_timer_wakeup(sleepTime * 1000000); // wait for refreshRate seconds before next loop
          sleepTime = 0;
          esp_light_sleep_start();
        }
      }
    }
  }
}