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

void remove_char(char *str, char c)
{
    int i, j;
    int len = strlen(str);
    for (i = j = 0; i < len; i++)
    {
        if (str[i] != c)
        {
            str[j++] = str[i];
        }
    }
    str[j] = '\0';
}