#ifndef MQTT_H
#define MQTT_H
#include <PubSubClient.h>
#include <WiFi.h>

class Mqtt
{
public:
    Mqtt(PubSubClient *mqttClient);
    PubSubClient *mqttClient;
    int8_t connect(const char *serverHost, const uint16_t server_port, const char *user, const char *password);
    int8_t send(const char *topic, const char *payload);

private:
};

void callback(char *topic, byte *payload, unsigned int length);
#endif
