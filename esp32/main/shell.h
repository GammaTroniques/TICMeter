#ifndef SHELL_H
#define SHELL_H
#include <Arduino.h>
#include "config.h"
#include "mqtt.h"

// https://docs.espressif.com/projects/esp-idf/en/latest/esp32c6/api-reference/kconfig.html#config-esp-console-uart

struct shell_t
{
    Config *config;
    Mqtt *mqtt;
};

void shellLoop(void *pvParameters);

#endif