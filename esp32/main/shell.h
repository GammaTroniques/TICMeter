#ifndef SHELL_H
#define SHELL_H
#include <Arduino.h>
#include "config.h"
#include "mqtt.h"

struct shell_t
{
    Config *config;
    Mqtt *mqtt;
};

void shellLoop(void *pvParameters);

#endif