#ifdef __cplusplus
extern "C" {
#endif

#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include "system_interface.h"

/*
 * Time conversion constants.
 */
#define NANOSECONDS_PER_MILLISECOND    ( 1000000L )    /**< @brief Nanoseconds per millisecond. */
#define MILLISECONDS_PER_SECOND        ( 1000L )       /**< @brief Milliseconds per second. */


void* system_malloc(size_t n)
{
    return malloc(n);
}

void* system_calloc(size_t n, size_t size)
{
    return calloc(n, size);
}

void  system_free(void *ptr)
{
    free(ptr);
}

uint32_t system_ticks( void )
{
    TickType_t ticks = xTaskGetTickCount() * portTICK_PERIOD_MS;
    return ( uint32_t ) ticks;
}

uint32_t system_timestamp()
{
    time_t now;
    // struct tm timeinfo;
    time(&now);
    // localtime_r(&now, &timeinfo);
    return (uint32_t)now;
}

void system_sleep( uint32_t time_ms )
{
    vTaskDelay(time_ms / portTICK_PERIOD_MS);
}

uint32_t system_random(void)
{
    return (uint32_t)(0xffffffff & rand());
}

#ifdef __cplusplus
}
#endif
