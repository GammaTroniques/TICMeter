#ifndef COMMON_H
#define COMMON_H

#include "linky.h"
#include "config.h"

void deleteTask(TaskHandle_t task);
void suspendTask(TaskHandle_t task);
void resumeTask(TaskHandle_t task);
void hard_reset();

#endif