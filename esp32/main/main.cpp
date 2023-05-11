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
#include "esp_adc/adc_oneshot.h"
#include "esp_adc/adc_cali.h"
#include "esp_adc/adc_cali_scheme.h"
#include "string.h"

// #include <ArduinoJson.h>
// #include <WiFi.h>
// #include <HTTPClient.h>
// #include <NTPClient.h>
// #include <WiFiUdp.h>

#include "linky.h"
#include "main.h"
#include "config.h"
#include "wifi.h"
#include "shell.h"
#include "mqtt.h"
#include "pairing.h"

#include "soc/rtc.h"
#include "esp_pm.h"

#define MILLIS xTaskGetTickCount() * portTICK_PERIOD_MS

const char *ntpServer = "pool.ntp.org";
const long gmtOffset_sec = 0;        // UTC
const int daylightOffset_sec = 3600; //

Linky linky(MODE_HISTORIQUE, 17, 16);
Config config;

// ------------Global variables stored in RTC memory to keep their values after deep sleep
RTC_NOINIT_ATTR LinkyData dataArray[15]; // 10 + 5 in case of error
RTC_NOINIT_ATTR unsigned int dataIndex = 0;
RTC_NOINIT_ATTR uint8_t firstBoot = 1;
char jsonPost[1024] = {0};
// ---------------------------------------------------------------------------------------

TaskHandle_t fetchLinkyDataTaskHandle = NULL;
TaskHandle_t pushButtonTaskHandle = NULL;
TaskHandle_t pairingTaskHandle = NULL;

adc_oneshot_unit_handle_t adc1_handle;

void led_blink_task(void *pvParameter)
{
  gpio_reset_pin(LED_RED);
  gpio_reset_pin(LED_GREEN);
  gpio_set_direction(LED_RED, GPIO_MODE_OUTPUT);
  gpio_set_direction(LED_GREEN, GPIO_MODE_OUTPUT);
  uint8_t led_red_state = 0;
  while (1)
  {
    // ESP_LOGI(TAG, "Turning the LED %s!", led_red_state == true ? "ON" : "OFF");
    // ESP_LOGI(TAG, "%lld", esp_log_timestamp());
    // ESP_LOGI(TAG, "%lu", xTaskGetTickCount());
    // ESP_LOGI(TAG, "%lu", );

    gpio_set_level(LED_RED, led_red_state);
    led_red_state = !led_red_state;
    vTaskDelay(950 / portTICK_PERIOD_MS);

    gpio_set_level(LED_RED, led_red_state);
    led_red_state = !led_red_state;
    vTaskDelay(50 / portTICK_PERIOD_MS);

    // gpio_set_level(LED_GREEN, getVUSB());
  }
}

void linkyRead(void *pvParameters);
void loop(void *arg)
{
  while (1)
  {
    int raw = 0;
    adc_oneshot_read(adc1_handle, V_CONDO_PIN, &raw);
    ESP_LOGI("LOOP", "VCondo: %d", raw);
    vTaskDelay(1000 / portTICK_PERIOD_MS);
  }
}

#define MAIN_TAG "MAIN"

// main
extern "C" void app_main(void)
{

  ESP_LOGI(MAIN_TAG, "Starting ESP32 Linky...");
  esp_log_level_set("wifi", ESP_LOG_ERROR);
  rtc_cpu_freq_config_t tmp;
  rtc_clk_cpu_freq_get_config(&tmp);
  ESP_LOGI(MAIN_TAG, "RTC CPU Freq: %lu", tmp.freq_mhz);
  rtc_cpu_freq_config_t conf;

  esp_pm_config_t pm_config = {
      .max_freq_mhz = 80,
      .min_freq_mhz = 10,
      .light_sleep_enable = false

  };
  ESP_ERROR_CHECK(esp_pm_configure(&pm_config));
  rtc_clk_cpu_freq_get_config(&tmp);
  ESP_LOGI(MAIN_TAG, "RTC CPU Freq: %lu", tmp.freq_mhz);

  xTaskCreate(led_blink_task, "led_blink_task", 10000, NULL, 1, NULL);

  esp_reset_reason_t reason = esp_reset_reason();
  if ((reason != ESP_RST_DEEPSLEEP) && (reason != ESP_RST_SW))
  {
    ESP_LOGI(MAIN_TAG, "Not deep sleep or software reset, init RTC memory");
    // init RTC memory if not deep sleep or software reset
    memset(&dataArray, 0, sizeof(dataArray));
    dataIndex = 0;
    firstBoot = 1;
  }

  initPins();
  ESP_LOGI(MAIN_TAG, "VCondo: %f", getVCondo());

  // set frequency of cpu to 10MHz
  // start shell task
  // disable brownout detector
  // disable wifi (sleep)
  // pinmode
  config.begin();
  config.values.refreshRate = 60;
  config.values.enableDeepSleep = false;
  // check vcondo and sleep if not ok
  if (!getVUSB() && config.values.enableDeepSleep && getVCondo() < 4.5)
  {
    ESP_LOGI(MAIN_TAG, "VCondo is too low, going to deep sleep");
    esp_sleep_enable_timer_wakeup(1 * 1000000);
    esp_deep_sleep_start();
  }

  shellInit(); // init shell

  // connect to wifi
  if (connectToWifi())
  {
    time_t time = getTimestamp();                // get timestamp from ntp server
    ESP_LOGI(MAIN_TAG, "Timestamp: %lld", time); // print timestamp
    if (config.values.mode == MODE_WEB)
    {
      getConfigFromServer(&config); // get config from server
    }
  }
  vTaskDelay(1000 / portTICK_PERIOD_MS);
  disconectFromWifi();

  // start linky fetch task
  xTaskCreate(fetchLinkyDataTask, "fetchLinkyDataTask", 8192, NULL, 1, &fetchLinkyDataTaskHandle); // start linky task
  xTaskCreate(linkyRead, "linkyRead", 8192, NULL, 2, NULL);
  xTaskCreate(sendDataTask, "sendDataTask", 8192, NULL, 10, NULL); // start send data task

  // start push button task
  xTaskCreate(pushButtonTask, "pushButtonTask", 8192, NULL, 1, &pushButtonTaskHandle); // start push button task

  xTaskCreate(loop, "loop", 10000, NULL, 1, NULL);
}

void linkyRead(void *pvParameters)
{
  char *RX_TASK_TAG = "RX_TASK";
  linky.begin(); // init linky
  while (1)
  {
    const int rxBytes = uart_read_bytes(UART_NUM_1, linky.buffer + linky.index, RX_BUF_SIZE - linky.index - 1, 50 / portTICK_PERIOD_MS);
    linky.index += rxBytes;
    if (rxBytes > 0)
    {
      // ESP_LOGI(RX_TASK_TAG, "Read %d, buffer: %d, %d", rxBytes, strlen(linky.buffer), linky.index);
    }
    vTaskDelay(100 / portTICK_PERIOD_MS);
  }
}

void sendDataTask(void *pvParameters)
{
  while (1)
  {
    if (dataIndex > 0)
    {
      char json[1024] = {0};
      preapareJsonData(dataArray, dataIndex, json, sizeof(json));
      ESP_LOGI(MAIN_TAG, "Sending data to server");
      if (connectToWifi())
      {
        ESP_LOGI(MAIN_TAG, "POST: %s", json);
        sendToServer(json, &config);
      }
      // connectToWifi();
      // ESP_LOGI(MAIN_TAG, "POST: %s", jsonPost);
      // sendToServer(jsonPost, &config);
      // vTaskDelay(10000 / portTICK_PERIOD_MS);
      disconectFromWifi();
      dataIndex = 0;
    }
    vTaskDelay(1000 / portTICK_PERIOD_MS);
  }
}

void fetchLinkyDataTask(void *pvParameters)
{
  while (1)
  {
    if (!linky.update())
    {
      ESP_LOGI(MAIN_TAG, "Linky update failed");
      vTaskDelay((config.values.refreshRate * 1000) / portTICK_PERIOD_MS); // wait for refreshRate seconds before next loop
      continue;
    }
    dataArray[0] = linky.data;
    dataArray[0].timestamp = getTimestamp();
    dataIndex = 1;
    ESP_LOGI(MAIN_TAG, "Data stored: %d - BASE: %ld", dataIndex, dataArray[0].BASE);
    // preapareJsonData(dataArray, dataIndex, jsonPost, sizeof(jsonPost));
    vTaskDelay((config.values.refreshRate * 1000) / portTICK_PERIOD_MS); // wait for refreshRate seconds before next loop
  }
}

void pushButtonTask(void *pvParameters)
{
  uint32_t startPushTime = 0;
  while (1)
  {
    if (gpio_get_level(PAIRING_PIN) == 0)
    {
      if (startPushTime == 0)
      {
        startPushTime = MILLIS;
      }
      else if (MILLIS - startPushTime > 2000)
      {
        startPushTime = 0;
        ESP_LOGI("PAIRING", "Pairing mode");
        /// setCpuFrequencyMhz(240);
        // Serial.begin(115200);
        // WiFi.setSleep(false);

        vTaskSuspend(fetchLinkyDataTaskHandle);

        uint16_t *time = (uint16_t *)malloc(sizeof(uint16_t));
        *time = 500;
        xTaskCreate(pairingLEDTask, "pairingLEDTask", 10000, time, 1, NULL);

        switch (config.values.connectionType)
        {
        case CONNECTION_TYPE_WIFI:
          start_captive_portal();
          break;
        case CONNECTION_TYPE_ZIGBEE:
          // startZigbeeConfig(config);
          break;
        default:
          break;
        }
        vTaskSuspend(pushButtonTaskHandle);
      }
    }
    else
    {
      startPushTime = 0;
      taskYIELD();
      vTaskDelay(100 / portTICK_PERIOD_MS);
    }
  }
}

void preapareJsonData(LinkyData *data, char dataIndex, char *json, unsigned int jsonSize)
{
  cJSON *jsonObject = cJSON_CreateObject();
  cJSON_AddStringToObject(jsonObject, "TOKEN", config.values.web.token);

  cJSON_AddNumberToObject(jsonObject, "VCONDO", getVCondo());

  cJSON *dataObject = cJSON_CreateArray();
  for (int i = 0; i < dataIndex; i++)
  {
    cJSON *dataItem = cJSON_CreateObject();
    cJSON_AddNumberToObject(dataItem, "DATE", data[i].timestamp);
    cJSON_AddNumberToObject(dataItem, "ADCO", data[i].ADCO);
    cJSON_AddStringToObject(dataItem, "OPTARIF", data[i].OPTARIF);
    cJSON_AddNumberToObject(dataItem, "ISOUSC", data[i].ISOUSC);
    if (data[i].BASE != 0)
      cJSON_AddNumberToObject(dataItem, "BASE", data[i].BASE);

    if (data[i].HCHC != 0)
      cJSON_AddNumberToObject(dataItem, "HCHC", data[i].HCHC);

    if (data[i].HCHP != 0)
      cJSON_AddNumberToObject(dataItem, "HCHP", data[i].HCHP);

    cJSON_AddStringToObject(dataItem, "PTEC", data[i].PTEC);
    cJSON_AddNumberToObject(dataItem, "IINST", data[i].IINST);
    cJSON_AddNumberToObject(dataItem, "IMAX", data[i].IMAX);
    cJSON_AddNumberToObject(dataItem, "PAPP", data[i].PAPP);
    cJSON_AddStringToObject(dataItem, "HHPHC", data[i].HHPHC);
    cJSON_AddStringToObject(dataItem, "MOTDETAT", data[i].MOTDETAT);
    cJSON_AddItemToArray(dataObject, dataItem);
  }

  if (dataIndex == 0)
  {
    // Send empty data to server to keep the connection alive
    cJSON_AddStringToObject(jsonObject, "ERROR", "Cant read data from linky");
    cJSON *dataItem = cJSON_CreateObject();
    cJSON_AddNumberToObject(dataItem, "DATE", getTimestamp());
    cJSON_AddNullToObject(dataItem, "BASE");
    cJSON_AddNullToObject(dataItem, "HCHC");
    cJSON_AddNullToObject(dataItem, "HCHP");
    cJSON_AddItemToArray(dataObject, dataItem);
  }
  cJSON_AddItemToObject(jsonObject, "data", dataObject);
  char *jsonString = cJSON_PrintUnformatted(jsonObject);
  strncpy(json, jsonString, jsonSize);
  free(jsonString);
  cJSON_Delete(jsonObject);
}

float getVCondo()
{
  int raw = 0;
  // adc_oneshot_io_to_channel
  ESP_ERROR_CHECK(adc_oneshot_read(adc1_handle, V_CONDO_PIN, &raw));
  ESP_LOGI(MAIN_TAG, "VCondo raw: %d", raw);
  float vCondo = (float)(raw * 5) / 3988; // get VCondo from ADC after voltage divider
  return vCondo;
}

uint8_t getVUSB()
{
  // return adc1_get_raw(ADC1_CHANNEL_3) > 3700 ? 1 : 0;
  return gpio_get_level(V_USB_PIN);
}

uint8_t sleep(int time)
{
  // esp_sleep_enable_timer_wakeup(time * 1000);
  // esp_deep_sleep_start();
  esp_restart();
}

void initPins()
{
  adc_oneshot_unit_init_cfg_t init_config1 = {
      .unit_id = ADC_UNIT_1,
  };
  adc_oneshot_chan_cfg_t config = {
      .atten = ADC_ATTEN_DB_0,
      .bitwidth = ADC_BITWIDTH_DEFAULT,
  };
  ESP_ERROR_CHECK(adc_oneshot_new_unit(&init_config1, &adc1_handle));
  ESP_ERROR_CHECK(adc_oneshot_config_channel(adc1_handle, V_CONDO_PIN, &config));
}