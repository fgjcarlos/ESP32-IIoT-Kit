// Host shim for the FreeRTOS mutex API. The host build is single threaded,
// so the mutex is a no-op that still reports success.
#pragma once

#include <stddef.h>

#include "freertos/FreeRTOS.h"

typedef struct {
    int unused;
} StaticSemaphore_t;

typedef void *SemaphoreHandle_t;

static inline SemaphoreHandle_t xSemaphoreCreateMutexStatic(StaticSemaphore_t *storage)
{
    (void)storage;
    return (SemaphoreHandle_t)(size_t)1;
}

static inline BaseType_t xSemaphoreTake(SemaphoreHandle_t handle, TickType_t ticks)
{
    (void)handle;
    (void)ticks;
    return pdTRUE;
}

static inline BaseType_t xSemaphoreGive(SemaphoreHandle_t handle)
{
    (void)handle;
    return pdTRUE;
}
