#ifndef _MULTI_TIMER_H_
#define _MULTI_TIMER_H_

#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus  
extern "C" {  
#endif

typedef uint32_t (*PlatformTicksFunction_t)(void);

typedef struct MultiTimerHandle MultiTimer;

typedef void (*MultiTimerCallback_t)(MultiTimer* timer, void* userData);

struct MultiTimerHandle {
    MultiTimer* next;
    uint32_t deadline;
    uint32_t period;
    MultiTimerCallback_t callback;
    void* userData;
};

int MultiTimerInstall(PlatformTicksFunction_t ticksFunc);

int MultiTimerInit(MultiTimer* timer, uint32_t period, MultiTimerCallback_t cb, void* userData);

int MultiTimerStart(MultiTimer* timer, uint32_t startTime);

int MultiTimerStop(MultiTimer* timer);

bool MultiTimerActivated(MultiTimer* timer);

void MultiTimerYield(void);

#ifdef __cplusplus
} 
#endif

#endif
