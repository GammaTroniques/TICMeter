#include <Arduino.h>
#include <HardwareSerial.h>
#include <WiFi.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>
#include <NTPClient.h>
#include <WiFiUdp.h>
#include <EEPROM.h> //store data in flash memory (the ESP32 does not have EEPROM)
#include "linky.h"

#include "soc/soc.h"          //disable brownour problems
#include "soc/rtc_cntl_reg.h" //disable brownour problems

#define EEPROM_SIZE 512

#define uS_TO_S_FACTOR 1000000
#define V_CONDO_PIN 32 // io32

// #define OVERWRITE_CONFIG // to overwrite EEPROM with the config below (use only once to set the config)

// firt time config
// struct config_t
// {
//   char ssid[50] = "Livebox 2ryz";
//   char password[50] = "aaaaaaaaaaaaa1";
//   char serverHost[50] = "192.168.43.233:3001";
//   char postUrl[50] = "/post";
//   char configUrl[20] = "/config";
//   char version[10] = "";
//   unsigned int refreshRate = 60;
//   char deepSleep = 0;
//   char token[50] = "abc";
//  unsigned int dataCount = 3;
// };

struct config_t
{
  char ssid[50] = "";
  char password[50] = "";
  char serverHost[50] = "";
  char postUrl[50] = "";
  char configUrl[20] = "";
  char version[10] = "";
  unsigned int refreshRate = 60;
  char deepSleep = 0;
  char token[50] = "";
  unsigned int dataCount = 3;
};

config_t config;

const char *ntpServer = "pool.ntp.org";
const long gmtOffset_sec = 0;        // UTC
const int daylightOffset_sec = 3600; //

Linky linky(MODE_HISTORIQUE, 16, 17);

// ------------Global variables stored in RTC memory to keep their values after deep sleep
RTC_DATA_ATTR LinkyData dataArray[15]; // 10 + 5 in case of error
RTC_DATA_ATTR unsigned int dataIndex = 0;
// ---------------------------------------------------------------------------------------

float getVCondo()
{
  float vCondo = (float)(analogRead(V_CONDO_PIN) * 5) / 3988; // return voltage in V after the voltage divider
  return vCondo;
}

unsigned long getTimestamp()
{
  time_t now;
  struct tm timeinfo;
  unsigned int nTry = 0;
  while (!getLocalTime(&timeinfo) && nTry < 3)
  {
    Serial.println("Failed to obtain time : " + String(nTry));
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
  HTTPClient http;
  char url[100] = {0};
  createHttpUrl(url, config.serverHost, config.configUrl);
  strcat(url, "?token=");
  strcat(url, config.token);
  Serial.println(url);
  http.begin(url);
  int httpCode = http.GET();
  Serial.println(httpCode);
  if (httpCode == 200)
  {
    String payload = http.getString();
    Serial.println(payload);
    StaticJsonDocument<500> doc;
    deserializeJson(doc, payload);
    strcpy(config.ssid, doc["SSID"].as<const char *>());
    strcpy(config.password, doc["PASSWORD"].as<const char *>());
    strcpy(config.serverHost, doc["SERVER_HOST"].as<const char *>());
    strcpy(config.postUrl, doc["POST_URL"].as<const char *>());
    strcpy(config.configUrl, doc["CONFIG_URL"].as<const char *>());
    strcpy(config.version, doc["VERSION"].as<const char *>());
    strcpy(config.token, doc["TOKEN"].as<const char *>());
    config.refreshRate = doc["REFRESH_RATE"].as<unsigned int>();
    config.deepSleep = doc["DEEPSLEEP"].as<char>();
    config.dataCount = doc["DATA_COUNT"].as<unsigned int>();

    EEPROM.put(0, config);
    EEPROM.commit();
  }
  http.end();
  configTime(gmtOffset_sec, daylightOffset_sec, ntpServer);
  getTimestamp();
}

void preapareJsonData(LinkyData *data, char dataIndex, char *json, unsigned int jsonSize)
{
  DynamicJsonDocument doc(1024);
  doc["TOKEN"] = "abc";
  doc["VCONDO"] = getVCondo();

  for (int i = 0; i < dataIndex; i++)
  {
    Serial.println("Sending data " + String(dataIndex) + " " + String(data->timestamp));

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
  Serial.println(json);
}

char sendToServer(char *json)
{
  if (WiFi.status() == WL_CONNECTED)
  {
    WiFiClient client;
    HTTPClient http;

    char POST_URL[100] = {0};
    createHttpUrl(POST_URL, config.serverHost, config.postUrl);
    http.begin(client, POST_URL);
    http.addHeader("Content-Type", "application/json");

    int httpCode = http.POST(json);
    Serial.print("HTTP Code: ");
    Serial.println(httpCode);
    http.end();
    return httpCode;
  }
  return -1;
}

char connectToWifi()
{
  setCpuFrequencyMhz(240);                  // 240 MHz
  Serial.begin(115200);                     // Initialize serial port
  WiFi.begin(config.ssid, config.password); // Connect to the network
  Serial.println(config.ssid);
  Serial.println(config.password);
  unsigned long timeout = millis() + 5000;
  Serial.println("WiFi connecting...");
  while (WiFi.status() != WL_CONNECTED && millis() < timeout) // Wait for the Wi-Fi to connect or timeout
  {
  }

  if (WiFi.status() != WL_CONNECTED)
  {
    Serial.println("Failed to connect to WiFi");
    esp_sleep_enable_timer_wakeup(3 * 60 * uS_TO_S_FACTOR);
    esp_deep_sleep_start();
    return 0;
  }
  else
  {
    Serial.println("WiFi connected");
    Serial.println("IP address: ");
    Serial.println(WiFi.localIP());
    return 1;
  }
}

void disconectFromWifi()
{
  WiFi.disconnect();
  WiFi.mode(WIFI_OFF);
  WiFi.setSleep(true);
  setCpuFrequencyMhz(10);
  Serial.begin(115200);
}

void setup()
{
  setCpuFrequencyMhz(10);
  Serial.begin(115200);
  Serial.println("Starting...");
  WRITE_PERI_REG(RTC_CNTL_BROWN_OUT_REG, 0); // disable brownout detector in the setup.
  WiFi.setSleep(true);                       // disable wifi to save power

  delay(1000);
  pinMode(V_CONDO_PIN, INPUT);

  EEPROM.begin(EEPROM_SIZE); // init eeprom

#ifdef OVERWRITE_CONFIG
  EEPROM.put(0, config);
  EEPROM.commit();
#endif

  EEPROM.get(0, config); // read config from eeprom

  if (getVCondo() < 4.5)
  {
    Serial.println("VCondo too low, going to sleep");
    esp_sleep_enable_timer_wakeup(1 * 60 * uS_TO_S_FACTOR); // 1 minutes
    esp_deep_sleep_start();
  }
  if (connectToWifi())
  {
    getConfigFromServer();
    disconectFromWifi();
    linky.begin();
  }
}

void loop()
{
  char result = -1; // 0 = error, 1 = success, -1 = init
  char nTry = 0;
  do
  {
    delay(4000);             // wait to get some frame from linky into the serial buffer
    result = linky.update(); // decode the frame
    nTry++;
  } while (result != 1 && nTry < 10); // wait for a successfull frame

  if (dataIndex < 15) // store data until buffer is full
  {
    dataArray[dataIndex] = linky.data;               // store data
    dataArray[dataIndex].timestamp = getTimestamp(); // add timestamp
    Serial.println(dataArray[dataIndex].timestamp);
    dataIndex++; // increment index
    Serial.println("Data stored: " + String(dataIndex) + " - " + String(dataArray[dataIndex - 1].BASE));
  }
  else // buffer full
  {
    // shift data to the left
    for (int i = 0; i < 14; i++)
    {
      dataArray[i] = dataArray[i + 1];
    }
    dataArray[14] = linky.data;               // store data
    dataArray[14].timestamp = getTimestamp(); // add timestamp
    Serial.println("Data stored: " + String(dataIndex) + " - " + String(dataArray[14].BASE));
  }

  if ((dataIndex >= config.dataCount || nTry >= 10) && getVCondo() > 4.5) // send data if buffer contains at least 3 messages and VCondo is ok
  {
    char json[1024] = {0};
    preapareJsonData(dataArray, dataIndex, json, sizeof(json)); // prepare json data
    connectToWifi();                                            // reconnect to wifi
    getConfigFromServer();                                      // get config from server
    if (sendToServer(json) == 200)                              // send data
    {
      dataIndex = 0; // reset index
    }
    disconectFromWifi(); // disconnect from wifi when buffer is empty or 3 tries
    linky.begin();       // the serial communication with linky: when we change the CPU frequency, we need to reinit the serial communication
  }
  nTry = 0;
  delay(config.refreshRate * 1000);
}