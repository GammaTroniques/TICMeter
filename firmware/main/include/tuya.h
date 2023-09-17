#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "Linky.h"

#ifndef TUYA_H
#define TUYA_H

extern TaskHandle_t tuyaTaskHandle;

typedef enum
{
    STATE_IDLE,
    STATE_START,
    STATE_DATA_LOAD,
    STATE_TOKEN_PENDING,
    STATE_ACTIVATING,
    STATE_STARTUP_UPDATE,
    STATE_MQTT_CONNECT_START,
    STATE_MQTT_CONNECTING,
    STATE_MQTT_RECONNECT,
    STATE_MQTT_YIELD,
    STATE_RESTART,
    STATE_RESET,
    STATE_STOP,
    STATE_EXIT,
} tuya_run_state_t;
#define STATE_ID2STR(S) \
    ((S) == STATE_IDLE ? "STATE_IDLE" : ((S) == STATE_START ? "STATE_START" : ((S) == STATE_DATA_LOAD ? "STATE_DATA_LOAD" : ((S) == STATE_TOKEN_PENDING ? "STATE_TOKEN_PENDING" : ((S) == STATE_ACTIVATING ? "STATE_ACTIVATING" : ((S) == STATE_STARTUP_UPDATE ? "STATE_STARTUP_UPDATE" : ((S) == STATE_MQTT_CONNECT_START ? "STATE_MQTT_CONNECT_START" : ((S) == STATE_MQTT_CONNECTING ? "STATE_MQTT_CONNECTING" : ((S) == STATE_MQTT_RECONNECT ? "STATE_MQTT_RECONNECT" : ((S) == STATE_MQTT_YIELD ? "STATE_MQTT_YIELD" : ((S) == STATE_RESTART ? "STATE_RESTART" : ((S) == STATE_RESET ? "STATE_RESET" : ((S) == STATE_EXIT ? "STATE_EXIT" : "Unknown")))))))))))))
void init_tuya();
uint8_t send_tuya_data(LinkyData *linky);
void reset_tuya();

uint8_t tuya_waiting_bind();

#endif