
#include <Arduino.h> //not needed in the arduino ide
#include <HardwareSerial.h>
#include <ArduinoJson.h>
// Captive Portal
#include <DNSServer.h>
#include <esp_wifi.h>          //Used for mpdu_rx_disable android workaround
#include <AsyncTCP.h>          //https://github.com/me-no-dev/AsyncTCP using the latest dev version from @me-no-dev
#include <ESPAsyncWebServer.h> //https://github.com/me-no-dev/ESPAsyncWebServer using the latest dev version from @me-no-dev
#include <SPIFFS.h>

#include "config.h"
#include "common.h"

#define PARAM_WIFI_SSID "wifi-ssid"
#define PARAM_WIFI_PASSWORD "wifi-password"
#define PARAM_SERVER_MODE "server-mode"
#define PARAM_WEB_URL "web-url"

const char *ssid = AP_SSID;
const char *password = NULL; // no password

#define MAX_CLIENTS 4  // ESP32 supports up to 10 but I have not tested it yet
#define WIFI_CHANNEL 6 // 2.4ghz channel 6 https://en.wikipedia.org/wiki/List_of_WLAN_channels#2.4_GHz_(802.11b/g/n/ax)

const IPAddress localIP(4, 3, 2, 1);          // the IP address the web server, Samsung requires the IP to be in public space
const IPAddress gatewayIP(4, 3, 2, 1);        // IP address of the network should be the same as the local IP for captive portals
const IPAddress subnetMask(255, 255, 255, 0); // no need to change: https://avinetworks.com/glossary/subnet-mask/

const String localIPURL = "http://4.3.2.1"; // a string version of the local IP with http, used for redirecting clients to your webpage
DNSServer dnsServer;
AsyncWebServer server(80);

String htmlParser(const String &var)
{
    Serial.println(var);
    if (var == "TOTO")
    {
        return String(millis());
    }
    return String();
}

void startWifiConfig(Config config)
{
    WiFi.mode(WIFI_AP); // access point mode
    WiFi.softAPConfig(localIP, gatewayIP, subnetMask);
    WiFi.softAP(ssid, password, WIFI_CHANNEL, 0, MAX_CLIENTS);

    dnsServer.setTTL(300);             // set 5min client side cache for DNS
    dnsServer.start(53, "*", localIP); // if DNSServer is started with "*" for domain name, it will reply with provided IP to all DNS request

    // ampdu_rx_disable android workaround see https://github.com/espressif/arduino-esp32/issues/4423
    esp_wifi_stop();
    esp_wifi_deinit();

    wifi_init_config_t my_config = WIFI_INIT_CONFIG_DEFAULT(); // We use the default config ...
    my_config.ampdu_rx_enable = false;                         //... and modify only what we want.

    esp_wifi_init(&my_config); // set the new config
    esp_wifi_start();          // Restart WiFi
    delay(100);                // this is necessary don't ask me why

    // Required
    server.on("/connecttest.txt", [](AsyncWebServerRequest *request)
              { request->redirect("http://logout.net"); }); // windows 11 captive portal workaround
    server.on("/wpad.dat", [](AsyncWebServerRequest *request)
              { request->send(404); }); // Honestly don't understand what this is but a 404 stops win 10 keep calling this repeatedly and panicking the esp32 :)

    // Background responses: Probably not all are Required, but some are. Others might speed things up?
    // A Tier (commonly used by modern systems)
    server.on("/generate_204", [](AsyncWebServerRequest *request)
              { request->redirect(localIPURL); }); // android captive portal redirect
    server.on("/redirect", [](AsyncWebServerRequest *request)
              { request->redirect(localIPURL); }); // microsoft redirect
    server.on("/hotspot-detect.html", [](AsyncWebServerRequest *request)
              { request->redirect(localIPURL); }); // apple call home
    server.on("/canonical.html", [](AsyncWebServerRequest *request)
              { request->redirect(localIPURL); }); // firefox captive portal call home
    server.on("/success.txt", [](AsyncWebServerRequest *request)
              { request->send(200); }); // firefox captive portal call home
    server.on("/ncsi.txt", [](AsyncWebServerRequest *request)
              { request->redirect(localIPURL); }); // windows call home

    // B Tier (uncommon)
    //  server.on("/chrome-variations/seed",[](AsyncWebServerRequest *request){request->send(200);}); //chrome captive portal call home
    //  server.on("/service/update2/json",[](AsyncWebServerRequest *request){request->send(200);}); //firefox?
    //  server.on("/chat",[](AsyncWebServerRequest *request){request->send(404);}); //No stop asking Whatsapp, there is no internet connection
    //  server.on("/startpage",[](AsyncWebServerRequest *request){request->redirect(localIPURL);});

    // return 404 to webpage icon
    server.on("/favicon.ico", [](AsyncWebServerRequest *request)
              { request->send(404); }); // webpage icon

    // Initialize SPIFFS
    if (!SPIFFS.begin(true))
    {
        Serial.println("An Error has occurred while mounting SPIFFS");
        return;
    }
    server.on("/", HTTP_GET, [](AsyncWebServerRequest *request)
              { 
                request->send(SPIFFS, "/index.html", String(), false, htmlParser); 
                Serial.println("Served Basic HTML Page"); });

    server.serveStatic("/", SPIFFS, "/");

    server.on("/save-config", HTTP_POST, [&config](AsyncWebServerRequest *request)
              {

                if (request->hasParam(PARAM_WIFI_SSID, true))
                {
                    String ssid = request->getParam(PARAM_WIFI_SSID, true)->value();
                    strcpy(config.values.ssid, ssid.c_str());
                    Serial.printf("SSID: %s\n", config.values.ssid);
                }

                if (request->hasParam(PARAM_WIFI_PASSWORD, true))
                {
                    String password = request->getParam(PARAM_WIFI_PASSWORD, true)->value();
                    strcpy(config.values.password, password.c_str());
                    Serial.printf("Password: %\n", config.values.password);
                }

                if (request->hasParam(PARAM_SERVER_MODE, true))
                {
                    String mode = request->getParam(PARAM_SERVER_MODE, true)->value();
                }

                if (request->hasParam(PARAM_WEB_URL, true))
                {
                    String url = request->getParam(PARAM_WEB_URL, true)->value();
                    strcpy(config.values.web.host, url.c_str());
                    Serial.printf("Server URL: %s\n", config.values.web.host);
                }
                config.write();
                // request->send(200); 
                request->redirect(localIPURL); });

    // the catch all
    server.onNotFound([](AsyncWebServerRequest *request)
                      {
    request->redirect(localIPURL);
    Serial.print("onnotfound ");
    Serial.print(request->host());       //This gives some insight into whatever was being requested on the serial monitor
    Serial.print(" ");
    Serial.print(request->url());
    Serial.print(" sent redirect to " + localIPURL +"\n"); });

    server.begin();
}

void wifiConfigLoop(Config *config)
{
    // dnsServer.processNextRequest();
    delay(1);
}