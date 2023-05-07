#ifndef MQTT_H
#define MQTT_H

#include "config.h"
#include <stdio.h>
#include "esp_log.h"
#include "mqtt_client.h"
#include "esp_event.h"

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"
#include "freertos/queue.h"

#include "esp_log.h"
#include "linky.h"

static void mqtt_event_handler(void *handler_args, esp_event_base_t base, int32_t event_id, void *event_data);
void mqtt_app_start(void);

void setupHomeAssistantDiscovery();
void sendToMqtt(LinkyData *linky);

extern esp_mqtt_client_handle_t mqttClient;

#endif
