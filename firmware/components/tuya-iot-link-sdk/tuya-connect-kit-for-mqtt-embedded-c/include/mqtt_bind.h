#ifndef __MQTT_BIND_H_
#define __MQTT_BIND_H_

#include "tuya_iot.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    STATE_MQTT_BIND_START,
    STATE_MQTT_BIND_CONNECT,
    STATE_MQTT_BIND_COMPLETE,
    STATE_MQTT_BIND_TIMEOUT,
    STATE_MQTT_BIND_FAILED,
    STATE_MQTT_BIND_EXIT,
    STATE_MQTT_BIND_CONNECTED_WAIT,
    STATE_MQTT_BIND_TOKEN_WAIT,
} mqtt_bind_state_t;

extern mqtt_bind_state_t mqtt_bind_state;


int mqtt_bind_token_get(const tuya_iot_config_t* config, tuya_binding_info_t* binding);

#ifdef __cplusplus
}
#endif
#endif
