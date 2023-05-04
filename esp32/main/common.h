#ifndef COMMON_H
#define COMMON_H

#include <PubSubClient.h>
#include <WiFi.h>
#include "mqtt.h"

WiFiClient espClient;
PubSubClient mqttClient(espClient);
Mqtt mqtt(&mqttClient);

#endif