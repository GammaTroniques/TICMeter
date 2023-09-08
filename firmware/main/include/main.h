#ifndef MAIN_H
#define MAIN_H

#include "linky.h"

#define uS_TO_S_FACTOR 1000000

extern TaskHandle_t fetchLinkyDataTaskHandle;
extern TaskHandle_t sendDataTaskHandle;

void fetchLinkyDataTask(void *pvParameters);

#endif