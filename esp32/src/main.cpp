#include <Arduino.h>
#include <HardwareSerial.h>
#include <WiFi.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>
#include <NTPClient.h>
#include <WiFiUdp.h>
#include "linky.h"

const char *ssid = "Livebox 2ryz";
const char *password = "aaaaaaaaaaaaa1";

#define SERVER_HOST "http://192.168.43.233:3001"
#define POST_URL "/post"
#define CONFIG_URL "/config"
#define uS_TO_S_FACTOR 1000000

#define V_CONDO_PIN 32 // io32

const char *ntpServer = "pool.ntp.org";
const long gmtOffset_sec = 0;
const int daylightOffset_sec = 3600;

Linky linky(MODE_HISTORIQUE, 16, 17);

// ------------Global variables stored in RTC memory to keep their values after deep sleep
RTC_DATA_ATTR int bootCount = 0;
RTC_DATA_ATTR LinkyData dataArray[15]; // 10 + 5 in case of error
RTC_DATA_ATTR unsigned int dataIndex = 0;
RTC_DATA_ATTR unsigned int refreshRateMin = 1;
// ---------------------------------------------------------------------------------------

float getVCondo()
{
  float vCondo = analogRead(V_CONDO_PIN) * 5 / 3988;
  return vCondo;
}

unsigned long getTimestamp()
{
  time_t now;
  struct tm timeinfo;
  if (!getLocalTime(&timeinfo))
  {
    Serial.println("Failed to obtain time");
  }
  time(&now);
  return now;
}

void getConfig()
{
  HTTPClient http;
  http.begin(SERVER_HOST CONFIG_URL);
  int httpCode = http.GET();
  if (httpCode > 0)
  {
    String payload = http.getString();
    Serial.println(payload);
    StaticJsonDocument<200> doc;
    deserializeJson(doc, payload);
    const char *ssid = doc["SSID"];
    const char *password = doc["PASSWORD"];
    refreshRateMin = doc["REFRESH_RATE"];
    Serial.println(ssid);
    Serial.println(password);
    Serial.println(refreshRateMin);
  }
  http.end();
  configTime(gmtOffset_sec, daylightOffset_sec, ntpServer);
  getTimestamp();
}

char sendTo(LinkyData data, char nMsg)
{
  static char firstTime = 1;
  if (WiFi.status() == WL_CONNECTED)
  {
    WiFiClient client;
    HTTPClient http;

    http.begin(client, SERVER_HOST POST_URL);

    http.addHeader("Content-Type", "application/json");

    DynamicJsonDocument doc(1024);

    Serial.println("Time " + String(getTimestamp()));

    doc["nMSG"] = nMsg;
    doc["DATE"] = data.timestamp;
    doc["TOKEN"] = "abc";
    doc["ADCO"] = data.ADCO;
    doc["OPTARIF"] = data.OPTARIF;
    doc["ISOUSC"] = data.ISOUSC;

    if (data.BASE != 0)
      doc["BASE"] = data.BASE;

    if (data.HCHC != 0)
      doc["HCHC"] = data.HCHC;

    if (data.HCHP != 0)
      doc["HCHP"] = data.HCHP;

    doc["PTEC"] = data.PTEC;
    doc["IINST"] = data.IINST;
    doc["IMAX"] = data.IMAX;
    doc["PAPP"] = data.PAPP;
    doc["HHPHC"] = data.HHPHC;
    doc["MOTDETAT"] = data.MOTDETAT;

    if (firstTime == 1)
    { // only send V_CONDO on last message
      doc["V_CONDO"] = getVCondo();
      firstTime = 0;
    }

    if(nMsg == 0){
      firstTime = 1;
    }

    char json[1024] = {0};
    serializeJson(doc, json);

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
  setCpuFrequencyMhz(240);
  Serial.begin(115200);

  WiFi.begin(ssid, password);
  unsigned long timeout = millis() + 7000;
  Serial.println("WiFi connecting...");
  while (WiFi.status() != WL_CONNECTED && millis() < timeout)
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
  WiFi.setSleep(true);
  pinMode(V_CONDO_PIN, INPUT);
  if (connectToWifi())
  {
    getConfig();
    disconectFromWifi();
    linky.begin();
  }
}

void loop()
{
  char result = -1; // 0 = error, 1 = success, -1 = init
  do
  {
    delay(5000);             // wait to get some frame from linky into the serial buffer
    result = linky.update(); // decode the frame
  } while (result != 1);     // wait for a successfull frame

  if (dataIndex < 15) // store data until buffer is full
  {
    dataArray[dataIndex] = linky.data;               // store data
    dataArray[dataIndex].timestamp = getTimestamp(); // add timestamp
    dataIndex++;                                     // increment index
    Serial.println("Data stored: " + String(dataIndex) + " - " + String(dataArray[dataIndex - 1].BASE));
  }
  else // buffer full
  {
    // no nothing
  }

  if (dataIndex >= 3) // send data if buffer contains at least 3 messages
  {
    connectToWifi();                  // reconnect to wifi
    getConfig();                      // get config from server
    int nTry = 0;                     // number of tries
    while (dataIndex > 0 && nTry < 3) // send data until buffer is empty or 3 tries
    {
      if (sendTo(dataArray[dataIndex - 1], dataIndex - 1) == 200) // send data and check if success
      {
        Serial.println("Data sent: " + String(dataIndex - 1) + " - " + String(dataArray[dataIndex - 1].BASE));
        dataIndex--; // decrement index to send next data
      }
      else
      {
        nTry++; // increment number of tries if error
      }
    }
    disconectFromWifi(); // disconnect from wifi when buffer is empty or 3 tries
    linky.begin();       // the serial communication with linky: when we change the CPU frequency, we need to reinit the serial communication
  }

  delay(refreshRateMin * 60 * 1000);
}