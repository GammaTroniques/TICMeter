
#include "mqtt.h"

Mqtt::Mqtt(PubSubClient *mqttClient)
{
    this->mqttClient = mqttClient;
}

void callback(char *topic, byte *payload, unsigned int length)
{
    Serial.print("Message arrived [");
    Serial.print(topic);
    Serial.print("] ");
    for (int i = 0; i < length; i++)
    {
        Serial.print((char)payload[i]);
    }
    Serial.println();
}

int8_t Mqtt::connect(const char *serverHost, const uint16_t server_port, const char *user, const char *password)
{
    mqttClient->setServer(serverHost, server_port);
    if (mqttClient->connect("Linky", user, password))
    {
        mqttClient->setCallback(callback);
        Serial.println("connected");
        mqttClient->subscribe("linky/config");
    }
    else
    {
        Serial.print("failed with state ");
        Serial.print(mqttClient->state());
        delay(2000);
    }
    return 0;
}

int8_t Mqtt::send(const char *topic, const char *payload)
{
    Serial.printf("Sending to topic %s: %s\n", topic, payload);
    mqttClient->publish(topic, payload);
    return 0;
}