// Host shim for the ESP-IDF error codes used by the modules under test.
// Only the codes the gateway sources actually reference are declared, with
// the same numeric values as esp_common/include/esp_err.h so the behaviour
// of the code under test does not change.
#pragma once

#include <stdint.h>

typedef int esp_err_t;

#define ESP_OK                  0
#define ESP_FAIL                -1
#define ESP_ERR_NO_MEM          0x101
#define ESP_ERR_INVALID_ARG     0x102
#define ESP_ERR_INVALID_STATE   0x103
#define ESP_ERR_INVALID_SIZE    0x104
#define ESP_ERR_NOT_FOUND       0x105
#define ESP_ERR_NOT_SUPPORTED   0x106
#define ESP_ERR_NVS_NOT_FOUND   0x1102

static inline const char *esp_err_to_name(esp_err_t err)
{
    (void)err;
    return "ESP_ERR";
}
