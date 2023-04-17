#include <Arduino.h>
#include <HardwareSerial.h>
#include <WiFi.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>
#include <NTPClient.h>
#include <WiFiUdp.h>

#include "wifiConfig.h"
#include "linky.h"
#include "main.h"
#include "config.h"

#include "common.h"

#include "soc/soc.h"          //disable brownour problems
#include "soc/rtc_cntl_reg.h" //disable brownour problems

#define uS_TO_S_FACTOR 1000000
#define V_CONDO_PIN 32 // io32

#define DEBUG // to allow serial communication

#ifdef DEBUG
#include "shell.h"
#endif

const char *ntpServer = "pool.ntp.org";
const long gmtOffset_sec = 0;        // UTC
const int daylightOffset_sec = 3600; //

Linky linky(MODE_HISTORIQUE, 16, 17);
Config config;

// ------------Global variables stored in RTC memory to keep their values after deep sleep
RTC_DATA_ATTR LinkyData dataArray[15]; // 10 + 5 in case of error
RTC_DATA_ATTR unsigned int dataIndex = 0;
// ---------------------------------------------------------------------------------------

void setup()
{
  setCpuFrequencyMhz(10); // set the CPU frequency to 10Mhz to save power
#ifdef DEBUG
  Serial.begin(115200); // init serial
  Serial.println("Starting...");

  shell_t data = {&config, &mqtt};
  xTaskCreate(shellLoop, "shellLoop", 8192, &data, 1, NULL);
#endif
  WRITE_PERI_REG(RTC_CNTL_BROWN_OUT_REG, 0); // disable brownout detector (to prevent reset when starting the WiFi)
  WiFi.setSleep(true);                       // disable wifi to save power

  delay(1000);
  pinMode(V_CONDO_PIN, INPUT);

  config.begin(); // init config

  if (config.values.enableDeepSleep && getVCondo() < 4.5) // if VCondo is too low, go to sleep for 1 minute to wait for the condo to charge
  {
#ifdef DEBUG
    Serial.println("VCondo too low, going to sleep");
#endif
    esp_sleep_enable_timer_wakeup(1 * 60 * uS_TO_S_FACTOR); // 1 minutes
    esp_deep_sleep_start();
  }

  if (1) // if VBUS is connected (charger plugged in)
  {
    setCpuFrequencyMhz(240);
    Serial.begin(115200);
    WiFi.setSleep(false);
    // startWifiConfig(config);
    connectToWifi();

    while (1)
    {
      wifiConfigLoop(&config);
    }
  }
  else if (connectToWifi())
  {
    getConfigFromServer();
    disconectFromWifi();
    linky.begin();
  }
}

void loop()
{
  //   char result = -1; // 0 = error, 1 = success, -1 = init
  //   char nTry = 0;    // number of tries to get a frame from linky
  //   do
  //   {
  //     delay(4000);             // wait to get some frame from linky into the serial buffer
  //     result = linky.update(); // decode the frame
  //     nTry++;
  //   } while (result != 1 && nTry < 10); // wait for a successfull frame

  //   if (dataIndex < 15) // store data until buffer is full
  //   {
  //     dataArray[dataIndex] = linky.data;               // store data
  //     dataArray[dataIndex].timestamp = getTimestamp(); // add timestamp
  // #ifdef DEBUG
  //     Serial.print("Data stored: ");
  //     Serial.print(dataIndex);
  //     Serial.print(" - BASE:");
  //     Serial.println(dataArray[dataIndex].BASE);
  // #endif
  //     dataIndex++; // increment index
  //   }
  //   else // buffer full
  //   {
  // #ifdef DEBUG
  //     Serial.print("Buffer full, shifting data");
  // #endif
  //     // shift data to the left
  //     for (int i = 0; i < 14; i++)
  //     {
  //       dataArray[i] = dataArray[i + 1];
  //     }
  //     dataArray[14] = linky.data;               // store data
  //     dataArray[14].timestamp = getTimestamp(); // add timestamp
  // #ifdef DEBUG
  //     Serial.print("Data stored: ");
  //     Serial.print(dataIndex);
  //     Serial.print(" - BASE:");
  //     Serial.println(dataArray[14].BASE);
  // #endif
  //   }

  //   if ((dataIndex >= config.dataCount || nTry >= 10) && getVCondo() > 4.5) // send data if buffer contains at least 3 messages, nTry >= 10 to avoid infinite loop and VCondo is ok
  //   {
  //     char json[1024] = {0};
  //     preapareJsonData(dataArray, dataIndex, json, sizeof(json)); // prepare json data
  //     connectToWifi();                                            // reconnect to wifi
  //     getConfigFromServer();                                      // get config from server
  //     if (sendToServer(json) == 200)                              // send data
  //     {
  //       // if data is sent, reset buffer
  //       dataIndex = 0; // reset index
  //     }
  //     disconectFromWifi(); // disconnect from wifi when buffer is empty or 3 tries
  //     linky.begin();       // the serial communication with linky: when we change the CPU frequency, we need to reinit the serial communication
  //   }
  //   nTry = 0;                         // reset nTry
  //   delay(config.refreshRate * 1000); // wait for refreshRate seconds before next loop
}

float getVCondo()
{
  float vCondo = (float)(analogRead(V_CONDO_PIN) * 5) / 3988; // get VCondo from ADC after voltage divider
  return vCondo;
}

unsigned long getTimestamp()
{
  time_t now;
  struct tm timeinfo;
  unsigned int nTry = 0;
  while (!getLocalTime(&timeinfo) && nTry < 3)
  {
#ifdef DEBUG
    Serial.println("Failed to obtain time : " + String(nTry) + "/3 try");
#endif
    delay(500);
    nTry++;
  }
  time(&now);
  return now;
}

void createHttpUrl(char *url, const char *host, const char *path)
{

  url = strcat(url, "http://");
  url = strcat(url, host);
  url = strcat(url, path);
}

void getConfigFromServer()
{
#ifdef DEBUG
  Serial.print("Getting config from server...");
#endif
  HTTPClient http;
  char url[100] = {0};
  createHttpUrl(url, config.values.web.host, config.values.web.configUrl);
  strcat(url, "?token=");
  strcat(url, config.values.web.token);
  http.begin(url);
  int httpCode = http.GET();
  if (httpCode == 200)
  {
    String payload = http.getString(); // Get the response payload
    StaticJsonDocument<500> doc;       // Create a document with a capacity of 500 bytes
    deserializeJson(doc, payload);     // Deserialize the JSON document

    //------------------Copy config from json to config struct------------------
    strcpy(config.values.ssid, doc["SSID"].as<const char *>());
    strcpy(config.values.password, doc["PASSWORD"].as<const char *>());
    strcpy(config.values.web.host, doc["SERVER_HOST"].as<const char *>());
    strcpy(config.values.web.postUrl, doc["POST_URL"].as<const char *>());
    strcpy(config.values.web.configUrl, doc["CONFIG_URL"].as<const char *>());
    strcpy(config.values.version, doc["VERSION"].as<const char *>());
    strcpy(config.values.web.token, doc["TOKEN"].as<const char *>());
    config.values.refreshRate = doc["REFRESH_RATE"].as<uint16_t>();
    config.values.enableDeepSleep = doc["DEEPSLEEP"].as<uint8_t>();
    config.values.dataCount = doc["DATA_COUNT"].as<uint8_t>();

    //--------------------------------------------------------------------------
    config.write();
#ifdef DEBUG
    Serial.println("OK");
#endif
  }
  else
  {
#ifdef DEBUG
    Serial.println("ERROR");
#endif
  }
  http.end(); // close connection

#ifdef DEBUG
  Serial.print("Getting time from NTP...");
#endif
  configTime(gmtOffset_sec, daylightOffset_sec, ntpServer);
  unsigned long time = getTimestamp();
#ifdef DEBUG
  if (time > 1000000000) // check if time is valid
  {

    Serial.println("OK");
  }
#endif
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
    doc["data"][0]["DATE"] = getTimestamp();
    doc["data"][0]["BASE"] = nullptr;
    doc["data"][0]["HCHC"] = nullptr;
    doc["data"][0]["HCHP"] = nullptr;
  }
  serializeJson(doc, json, jsonSize);
#ifdef DEBUG
  Serial.println(" OK");
#endif
}

char sendToServer(char *json)
{
#ifdef DEBUG
  Serial.print("Sending data to server... ");
#endif
  if (WiFi.status() == WL_CONNECTED)
  {
    WiFiClient client;
    HTTPClient http;

    char POST_URL[100] = {0};
    createHttpUrl(POST_URL, config.values.web.host, config.values.web.postUrl);
    http.begin(client, POST_URL);
    http.addHeader("Content-Type", "application/json");
    int httpCode = http.POST(json);
#ifdef DEBUG
    Serial.print("OK: ");
    Serial.println(httpCode);
#endif
    http.end();
    return httpCode;
  }
#ifdef DEBUG
  Serial.print("ERROR");
#endif
  return -1;
}

char connectToWifi()
{
  setCpuFrequencyMhz(240); // 240 MHz
#ifdef DEBUG
  Serial.begin(115200); // Initialize serial port
#endif
  WiFi.begin(config.values.ssid, config.values.password); // Connect to the network
#ifdef DEBUG
  Serial.printf("Connecting to %s ...\n", config.values.ssid);
#endif
  unsigned long timeout = millis() + 5000;
  while (WiFi.status() != WL_CONNECTED && millis() < timeout) // Wait for the Wi-Fi to connect or timeout
  {
  }

  if (WiFi.status() != WL_CONNECTED)
  {
#ifdef DEBUG
    Serial.println("Failed to connect to WiFi. Going to sleep for 1 minutes.");
#endif
    esp_sleep_enable_timer_wakeup(1 * 60 * uS_TO_S_FACTOR);
    esp_deep_sleep_start();
    return 0;
  }

#ifdef DEBUG
  Serial.println("WiFi connected");
  Serial.print("IP address: ");
  Serial.println(WiFi.localIP());
#endif
  return 1;
}

void disconectFromWifi()
{
#ifdef DEBUG
  Serial.print("Disconecting from wifi...");
#endif
  WiFi.disconnect();
  WiFi.mode(WIFI_OFF);
  WiFi.setSleep(true);
  setCpuFrequencyMhz(10);
#ifdef DEBUG
  Serial.begin(115200); // when we change the CPU frequency, we need to reinit the serial communication
#endif
#ifdef DEBUG
  Serial.println("OK");
#endif
}
