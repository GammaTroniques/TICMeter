#include "common.h"

void deleteTask(TaskHandle_t task)
{
    if (task != NULL)
    {
        vTaskDelete(task);
        task = NULL;
    }
}

void suspendTask(TaskHandle_t task)
{
    if (task != NULL)
    {
        vTaskSuspend(task);
    }
}

void resumeTask(TaskHandle_t task)
{
    if (task != NULL)
    {
        vTaskResume(task);
    }
}