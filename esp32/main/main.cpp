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

Linky linky(MODE_HISTORIQUE, 17, 16);
Config config;

#define MAX_DATA_INDEX 15 // 10 + 5 in case of error
// ------------Global variables stored in RTC memory to keep their values after deep sleep
RTC_DATA_ATTR LinkyData dataArray[MAX_DATA_INDEX];
RTC_DATA_ATTR unsigned int dataIndex = 0;
// RTC_DATA_ATTR uint8_t firstBoot = 1;
// ---------------------------------------------------------------------------------------

TaskHandle_t fetchLinkyDataTaskHandle = NULL;
TaskHandle_t pushButtonTaskHandle = NULL;
TaskHandle_t pairingTaskHandle = NULL;

#define MAIN_TAG "MAIN"

extern "C" void app_main(void)
{

  ESP_LOGI(MAIN_TAG, "Starting ESP32 Linky...");
  initPins();
  startLedPattern(PATTERN_START);
  setCPUFreq(10);
  xTaskCreate(pairingButtonTask, "pushButtonTask", 8192, NULL, 1, &pushButtonTaskHandle); // start push button task

  // esp_reset_reason_t reason = esp_reset_reason();
  // if ((reason != ESP_RST_DEEPSLEEP) && (reason != ESP_RST_SW))
  // {
  //   ESP_LOGI(MAIN_TAG, "Not deep sleep or software reset, init RTC memory");
  //   // init RTC memory if not deep sleep or software reset
  //   memset(&dataArray, 0, sizeof(dataArray));
  //   dataIndex = 0;
  //   firstBoot = 1;
  // }

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

  linky.begin();
  // ESP_LOGI(MAIN_TAG, "VCondo: %f", getVCondo());
  // check vcondo and sleep if not ok
  // if (!getVUSB() && config.values.enableDeepSleep && getVCondo() < 4.5)
  // {
  //   ESP_LOGI(MAIN_TAG, "VCondo is too low, going to deep sleep");
  //   esp_sleep_enable_timer_wakeup(1 * 1000000);
  //   esp_deep_sleep_start();
  // }

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
  default:
    break;
  }
  // start linky fetch task
  xTaskCreate(fetchLinkyDataTask, "fetchLinkyDataTask", 8192, NULL, 1, &fetchLinkyDataTaskHandle); // start linky task
}

void fetchLinkyDataTask(void *pvParameters)
{
  while (1)
  {
    if (!linky.update() ||
        (strlen(linky.data.ADCO) == 0))
    {
      ESP_LOGI(MAIN_TAG, "Linky update failed");
      startLedPattern(PATTERN_LINKY_ERR);
      linky.index = 0;
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
      ESP_LOGI(MAIN_TAG, "Data stored: %d - BASE: %ld", dataIndex, dataArray[0].BASE);
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
        sleep(config.values.refreshRate * 1000);
      }
      break;
    case MODE_MQTT:
    case MODE_MQTT_HA: // send data to mqtt server
    case MODE_TUYA:
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
    }
    vTaskDelay((abs(config.values.refreshRate - 5) * 1000) / portTICK_PERIOD_MS); // wait for refreshRate seconds before next loop
  }
}

uint8_t sleep(int timeMs)
{
  // esp_sleep_enable_timer_wakeup(timeMs * 1000);
  // esp_deep_sleep_start();
  return 0;
}
