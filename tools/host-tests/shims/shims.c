// Host implementations of the ESP-IDF symbols the shim headers declare.
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>

#include "esp_system.h"
#include "esp_timer.h"
#include "nvs.h"
#include "nvs_flash.h"

void esp_restart(void)
{
    fprintf(stderr, "esp_restart() called on the host: aborting\n");
    abort();
}

int64_t esp_timer_get_time(void)
{
    struct timespec now;
    clock_gettime(CLOCK_MONOTONIC, &now);
    return (int64_t)now.tv_sec * 1000000LL + (int64_t)(now.tv_nsec / 1000);
}

// --- in-memory NVS -------------------------------------------------------

#define NVS_MAX_KEYS 16

static struct {
    char key[64];
    uint64_t value;
    bool used;
} s_nvs[NVS_MAX_KEYS];

esp_err_t nvs_flash_init(void)
{
    return ESP_OK;
}

esp_err_t nvs_open(const char *name_space, nvs_open_mode_t mode, nvs_handle_t *out_handle)
{
    (void)name_space;
    (void)mode;
    if (out_handle == NULL) {
        return ESP_ERR_INVALID_ARG;
    }
    *out_handle = 1;
    return ESP_OK;
}

esp_err_t nvs_close(nvs_handle_t handle)
{
    (void)handle;
    return ESP_OK;
}

esp_err_t nvs_get_u64(nvs_handle_t handle, const char *key, uint64_t *out_value)
{
    (void)handle;
    if (key == NULL || out_value == NULL) {
        return ESP_ERR_INVALID_ARG;
    }
    for (int i = 0; i < NVS_MAX_KEYS; ++i) {
        if (s_nvs[i].used && strcmp(s_nvs[i].key, key) == 0) {
            *out_value = s_nvs[i].value;
            return ESP_OK;
        }
    }
    return ESP_ERR_NVS_NOT_FOUND;
}

esp_err_t nvs_set_u64(nvs_handle_t handle, const char *key, uint64_t value)
{
    (void)handle;
    if (key == NULL) {
        return ESP_ERR_INVALID_ARG;
    }
    for (int i = 0; i < NVS_MAX_KEYS; ++i) {
        if (s_nvs[i].used && strcmp(s_nvs[i].key, key) == 0) {
            s_nvs[i].value = value;
            return ESP_OK;
        }
    }
    for (int i = 0; i < NVS_MAX_KEYS; ++i) {
        if (!s_nvs[i].used) {
            s_nvs[i].used = true;
            s_nvs[i].value = value;
            snprintf(s_nvs[i].key, sizeof(s_nvs[i].key), "%s", key);
            return ESP_OK;
        }
    }
    return ESP_ERR_NO_MEM;
}

esp_err_t nvs_commit(nvs_handle_t handle)
{
    (void)handle;
    return ESP_OK;
}
