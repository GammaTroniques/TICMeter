#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/gpio.h"
#include "driver/uart.h"
#include "esp_log.h"
#include "esp_sleep.h"

#include "sdkconfig.h"
#include "driver/adc.h"
#include "esp_adc_cal.h"
#include "string.h"

#include <ArduinoJson.h>
// #include <WiFi.h>
// #include <HTTPClient.h>
// #include <NTPClient.h>
// #include <WiFiUdp.h>

#include "linky.h"
#include "main.h"
#include "config.h"

#define BLINK_GPIO 8
#define LED_RED (gpio_num_t)7
#define LED_GREEN (gpio_num_t)11
#define VUSB (gpio_num_t)3

const char *ntpServer = "pool.ntp.org";
const long gmtOffset_sec = 0;        // UTC
const int daylightOffset_sec = 3600; //

Linky linky(MODE_HISTORIQUE, 16, 17);
Config config;

// ------------Global variables stored in RTC memory to keep their values after deep sleep
RTC_DATA_ATTR LinkyData dataArray[15]; // 10 + 5 in case of error
RTC_DATA_ATTR unsigned int dataIndex = 0;
// ---------------------------------------------------------------------------------------

TaskHandle_t fetchLinkyDataTaskHandle = NULL;
TaskHandle_t pushButtonTaskHandle = NULL;
TaskHandle_t pairingTaskHandle = NULL;

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
    led_red_state = !led_red_state;
    gpio_set_level(LED_GREEN, getVUSB());
    gpio_set_level(LED_RED, led_red_state);
    vTaskDelay(200 / portTICK_PERIOD_MS);
  }
}

void rx_task(void *arg)
{
  const char *RX_TASK_TAG = "RX_TASK";
  esp_log_level_set(RX_TASK_TAG, ESP_LOG_INFO);
  while (1)
  {
    const int rxBytes = uart_read_bytes(UART_NUM_1, linky.buffer, RX_BUF_SIZE, 1 / portTICK_PERIOD_MS);
    if (rxBytes > 0)
    {
      ESP_LOGI(RX_TASK_TAG, "Read %d, strlen %d", rxBytes, strlen(linky.buffer));
    }
    vTaskDelay(50 / portTICK_PERIOD_MS);
  }
}

void loop(void *arg)
{
  while (1)
  {
    linky.update();
    linky.print();
    vTaskDelay(2000 / portTICK_PERIOD_MS);
  }
}

#define MAIN_TAG "MAIN"

extern "C" void app_main(void)
{
  ESP_LOGI(MAIN_TAG, "Starting ESP32 Linky...");
  // set frequency of cpu to 10MHz
  // start shell task
  // disable brownout detector
  // disable wifi (sleep)
  // pinmode
  config.begin();

  // check vcondo and sleep if not ok
  if (!getVUSB() && config.values.enableDeepSleep && getVCondo() < 4.5)
  {
    ESP_LOGI(MAIN_TAG, "VCondo is too low, going to deep sleep");
    esp_deep_sleep_start();
  }
  xTaskCreate(led_blink_task, "led_blink_task", 10000, NULL, 1, NULL);

  // start linky fetch task
  xTaskCreate(fetchLinkyDataTask, "fetchLinkyDataTask", 8192, NULL, 1, &fetchLinkyDataTaskHandle); // start linky task

  // start push button task
  xTaskCreate(pushButtonTask, "pushButtonTask", 8192, NULL, 1, &pushButtonTaskHandle); // start push button task

  // connect to wifi
  // get time from ntp
  // get config from server

  // xTaskCreate(rx_task, "uart_rx_task", 4096, NULL, configMAX_PRIORITIES, NULL);
  // xTaskCreate(loop, "loop", 10000, NULL, 1, NULL);
}

void fetchLinkyDataTask(void *pvParameters)
{
  linky.begin(); // init linky
  while (1)
  {
    char result = -1; // 0 = error, 1 = success, -1 = init
    char nTry = 0;    // number of tries to get a frame from linky
    do
    {
      vTaskDelay(4000);        // wait to get some frame from linky into the serial buffer
      result = linky.update(); // decode the frame
      nTry++;
    } while (result != 1 && nTry < 10); // wait for a successfull frame

    if (dataIndex < 15) // store data until buffer is full
    {
      dataArray[dataIndex] = linky.data; // store data
                                         ////////////////dataArray[dataIndex].timestamp = getTimestamp(); // add timestamp
#ifdef DEBUG
      Serial.print("Data stored: ");
      Serial.print(dataIndex);
      Serial.print(" - BASE:");
      Serial.println(dataArray[dataIndex].BASE);
#endif
      dataIndex++; // increment index
    }
    else // buffer full
    {
#ifdef DEBUG
      Serial.print("Buffer full, shifting data");
#endif
      // shift data to the left
      for (int i = 0; i < 14; i++)
      {
        dataArray[i] = dataArray[i + 1];
      }
      dataArray[14] = linky.data; // store data
                                  ///////////dataArray[14].timestamp = getTimestamp(); // add timestamp
#ifdef DEBUG
      Serial.print("Data stored: ");
      Serial.print(dataIndex);
      Serial.print(" - BASE:");
      Serial.println(dataArray[14].BASE);
#endif
    }

    if ((dataIndex >= config.values.dataCount || nTry >= 10) && getVCondo() > 4.5) // send data if buffer contains at least 3 messages, nTry >= 10 to avoid infinite loop and VCondo is ok
    {
      char json[1024] = {0};
      preapareJsonData(dataArray, dataIndex, json, sizeof(json)); // prepare json data
      connectToWifi();                                            // reconnect to wifi
      getConfigFromServer();                                      // get config from server
      if (sendToServer(json) == 200)                              // send data
      {
        // if data is sent, reset buffer
        dataIndex = 0; // reset index
      }
      disconectFromWifi(); // disconnect from wifi when buffer is empty or 3 tries
      linky.begin();       // the serial communication with linky: when we change the CPU frequency, we need to reinit the serial communication
    }
    nTry = 0;                                     // reset nTry
    vTaskDelay(config.values.refreshRate * 1000); // wait for refreshRate seconds before next loop
  }
}

void pushButtonTask(void *pvParameters)
{
  uint32_t startPushTime = 0;
  while (1)
  {
    // if (gpio_get_level(PAIRING_PIN) == 0)
    // {
    //   if (startPushTime == 0)
    //   {
    //     startPushTime = esp_timer_get_time();
    //   }
    //   else if (millis() - startPushTime > 2000)
    //   {
    //     startPushTime = 0;
    //     ESP_LOGI("PAIRING", "Pairing mode");
    //     /// setCpuFrequencyMhz(240);
    //     // Serial.begin(115200);
    //     WiFi.setSleep(false);

    //     uint16_t *time = (uint16_t *)malloc(sizeof(uint16_t));
    //     *time = 500;
    //     ///////////////////////////////////////xTaskCreate(pairingLEDTask, "pairingLEDTask", 10000, time, 1, NULL);

    //     switch (config.values.connectionType)
    //     {
    //     case CONNECTION_TYPE_WIFI:
    //       ///////////////////////////////////xTaskCreate(wifiPairingTask, "wifiPairingTask", 10000, &config, 1, &pairingTaskHandle);

    //       break;
    //     case CONNECTION_TYPE_ZIGBEE:
    //       // startZigbeeConfig(config);
    //       break;
    //     default:
    //       break;
    //     }
    //     vTaskSuspend(pushButtonTaskHandle);
    //   }
    // }
    // else
    // {
    //   startPushTime = 0;
    //   taskYIELD();
    //   vTaskDelay(100);
    // }
  }
}

void preapareJsonData(LinkyData *data, char dataIndex, char *json, unsigned int jsonSize)
{
#ifdef DEBUG
  Serial.print("Preparing json data...");
#endif
  DynamicJsonDocument doc(1024);
  doc["TOKEN"] = config.values.web.token;
  doc["VCONDO"] = getVCondo();

  for (int i = 0; i < dataIndex; i++)
  {
    doc["data"][i]["DATE"] = data[i].timestamp;
    doc["data"][i]["ADCO"] = data[i].ADCO;
    doc["data"][i]["OPTARIF"] = data[i].OPTARIF;
    doc["data"][i]["ISOUSC"] = data[i].ISOUSC;

    if (data[i].BASE != 0)
      doc["data"][i]["BASE"] = data[i].BASE;

    if (data[i].HCHC != 0)
      doc["data"][i]["HCHC"] = data[i].HCHC;

    if (data[i].HCHP != 0)
      doc["data"][i]["HCHP"] = data[i].HCHP;

    doc["data"][i]["PTEC"] = data[i].PTEC;
    doc["data"][i]["IINST"] = data[i].IINST;
    doc["data"][i]["IMAX"] = data[i].IMAX;
    doc["data"][i]["PAPP"] = data[i].PAPP;
    doc["data"][i]["HHPHC"] = data[i].HHPHC;
    doc["data"][i]["MOTDETAT"] = data[i].MOTDETAT;
  }

  if (dataIndex == 0)
  {
    // Send empty data to server to keep the connection alive
    doc["ERROR"] = "Cant read data from linky";
    //////////////////////////////doc["data"][0]["DATE"] = getTimestamp();
    doc["data"][0]["BASE"] = nullptr;
    doc["data"][0]["HCHC"] = nullptr;
    doc["data"][0]["HCHP"] = nullptr;
  }
  serializeJson(doc, json, jsonSize);
#ifdef DEBUG
  Serial.println(" OK");
#endif
}

float getVCondo()
{
  float vCondo = (float)(adc1_get_raw(V_CONDO_PIN) * 5) / 3988; // get VCondo from ADC after voltage divider
  return vCondo;
  return 3.3;
}

uint8_t getVUSB()
{
  return adc1_get_raw(ADC1_CHANNEL_3) > 3700 ? 1 : 0;
}
