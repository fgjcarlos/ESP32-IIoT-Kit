// Host shim for the FreeRTOS types the gateway sources use.
#pragma once

#include <stddef.h>

typedef int BaseType_t;
typedef unsigned int TickType_t;

#define pdTRUE  1
#define pdFALSE 0
#define pdPASS  pdTRUE
#define pdFAIL  pdFALSE
#define portMAX_DELAY 0xFFFFFFFFU
