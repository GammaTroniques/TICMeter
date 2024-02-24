#include "common.h"
#include "gpio.h"

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

void hard_restart()
{
    gpio_set_direction(RESET_PIN, GPIO_MODE_OUTPUT);
    gpio_set_level(RESET_PIN, 0);
    esp_restart();
}