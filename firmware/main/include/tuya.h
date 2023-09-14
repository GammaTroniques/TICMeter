#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "Linky.h"

#ifndef TUYA_H
#define TUYA_H

extern TaskHandle_t tuyaTaskHandle;

void init_tuya();
uint8_t send_tuya_data(LinkyData *linky);

#endif