// Host shim for nvs.h. The gateway only stores the bdSeq number, so the
// shim implements a single in-memory key/value store.
#pragma once

#include <stdint.h>

#include "esp_err.h"

typedef uint32_t nvs_handle_t;

typedef enum {
    NVS_READONLY = 0,
    NVS_READWRITE = 1,
} nvs_open_mode_t;

esp_err_t nvs_open(const char *name_space, nvs_open_mode_t mode, nvs_handle_t *out_handle);
esp_err_t nvs_close(nvs_handle_t handle);
esp_err_t nvs_get_u64(nvs_handle_t handle, const char *key, uint64_t *out_value);
esp_err_t nvs_set_u64(nvs_handle_t handle, const char *key, uint64_t value);
esp_err_t nvs_commit(nvs_handle_t handle);
