// Host implementations of the ESP-IDF symbols the shim headers declare.
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include "esp_system.h"
#include "esp_timer.h"
#include "nvs.h"
#include "nvs_flash.h"

#include "portal_auth.h"

void esp_restart(void)
{
    fprintf(stderr, "esp_restart() called on the host: aborting\n");
    abort();
}

// --- embedded brand logo (issue #12) --------------------------------------
//
// On the device the build embeds the logo of the selected brand profile with
// the symbol names below. The host build has no asset pipeline, so it
// provides a 2x2 RGB565 placeholder with the very same symbols: the code
// under test takes the same path in both places.
const uint8_t _binary_brand_logo_start[] = {0x00, 0x00, 0x00, 0x00,
                                            0x00, 0x00, 0x00, 0x00};
const uint32_t brand_logo_length = 8u;

int64_t esp_timer_get_time(void)
{
    struct timespec now;
    clock_gettime(CLOCK_MONOTONIC, &now);
    return (int64_t)now.tv_sec * 1000000LL + (int64_t)(now.tv_nsec / 1000);
}

// --- in-memory NVS -------------------------------------------------------
//
// Slots are kind-tagged (u64, u8, blob) so a key stored as one type is not
// readable as another, mirroring the device NVS behaviour closely enough for
// the portable rules under test.

#define NVS_MAX_KEYS 16
#define NVS_MAX_BLOB_LEN 64

typedef enum {
    NVS_SHIM_KIND_NONE = 0,
    NVS_SHIM_KIND_U64,
    NVS_SHIM_KIND_U8,
    NVS_SHIM_KIND_BLOB,
} nvs_shim_kind_t;

static struct {
    char key[64];
    bool used;
    nvs_shim_kind_t kind;
    uint64_t value;
    uint8_t blob[NVS_MAX_BLOB_LEN];
    size_t blob_len;
} s_nvs[NVS_MAX_KEYS];

static int nvs_shim_find(const char *key)
{
    for (int i = 0; i < NVS_MAX_KEYS; ++i) {
        if (s_nvs[i].used && strcmp(s_nvs[i].key, key) == 0) {
            return i;
        }
    }
    return -1;
}

static int nvs_shim_free(void)
{
    for (int i = 0; i < NVS_MAX_KEYS; ++i) {
        if (!s_nvs[i].used) {
            return i;
        }
    }
    return -1;
}

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
    int i = nvs_shim_find(key);
    if (i >= 0 && s_nvs[i].kind == NVS_SHIM_KIND_U64) {
        *out_value = s_nvs[i].value;
        return ESP_OK;
    }
    return ESP_ERR_NVS_NOT_FOUND;
}

esp_err_t nvs_set_u64(nvs_handle_t handle, const char *key, uint64_t value)
{
    (void)handle;
    if (key == NULL) {
        return ESP_ERR_INVALID_ARG;
    }
    int i = nvs_shim_find(key);
    if (i < 0) {
        i = nvs_shim_free();
        if (i < 0) {
            return ESP_ERR_NO_MEM;
        }
        s_nvs[i].used = true;
        snprintf(s_nvs[i].key, sizeof(s_nvs[i].key), "%s", key);
    }
    s_nvs[i].kind = NVS_SHIM_KIND_U64;
    s_nvs[i].value = value;
    return ESP_OK;
}

esp_err_t nvs_get_u8(nvs_handle_t handle, const char *key, uint8_t *out_value)
{
    (void)handle;
    if (key == NULL || out_value == NULL) {
        return ESP_ERR_INVALID_ARG;
    }
    int i = nvs_shim_find(key);
    if (i >= 0 && s_nvs[i].kind == NVS_SHIM_KIND_U8) {
        *out_value = (uint8_t)s_nvs[i].value;
        return ESP_OK;
    }
    return ESP_ERR_NVS_NOT_FOUND;
}

esp_err_t nvs_set_u8(nvs_handle_t handle, const char *key, uint8_t value)
{
    esp_err_t err = nvs_set_u64(handle, key, (uint64_t)value);
    if (err != ESP_OK) {
        return err;
    }
    int i = nvs_shim_find(key);
    if (i >= 0) {
        s_nvs[i].kind = NVS_SHIM_KIND_U8;
    }
    return ESP_OK;
}

esp_err_t nvs_get_blob(nvs_handle_t handle, const char *key, void *out_value,
                       size_t *length)
{
    (void)handle;
    if (key == NULL || out_value == NULL || length == NULL) {
        return ESP_ERR_INVALID_ARG;
    }
    int i = nvs_shim_find(key);
    if (i >= 0 && s_nvs[i].kind == NVS_SHIM_KIND_BLOB) {
        if (*length < s_nvs[i].blob_len) {
            return ESP_ERR_NVS_INVALID_LENGTH;
        }
        memcpy(out_value, s_nvs[i].blob, s_nvs[i].blob_len);
        *length = s_nvs[i].blob_len;
        return ESP_OK;
    }
    return ESP_ERR_NVS_NOT_FOUND;
}

esp_err_t nvs_set_blob(nvs_handle_t handle, const char *key, const void *value,
                       size_t length)
{
    (void)handle;
    if (key == NULL || (value == NULL && length > 0u)) {
        return ESP_ERR_INVALID_ARG;
    }
    if (length > NVS_MAX_BLOB_LEN) {
        return ESP_ERR_NO_MEM;
    }
    int i = nvs_shim_find(key);
    if (i < 0) {
        i = nvs_shim_free();
        if (i < 0) {
            return ESP_ERR_NO_MEM;
        }
        s_nvs[i].used = true;
        snprintf(s_nvs[i].key, sizeof(s_nvs[i].key), "%s", key);
    }
    s_nvs[i].kind = NVS_SHIM_KIND_BLOB;
    memcpy(s_nvs[i].blob, value, length);
    s_nvs[i].blob_len = length;
    return ESP_OK;
}

esp_err_t nvs_erase_key(nvs_handle_t handle, const char *key)
{
    (void)handle;
    if (key == NULL) {
        return ESP_ERR_INVALID_ARG;
    }
    int i = nvs_shim_find(key);
    if (i < 0) {
        return ESP_ERR_NVS_NOT_FOUND;
    }
    s_nvs[i].used = false;
    s_nvs[i].kind = NVS_SHIM_KIND_NONE;
    s_nvs[i].blob_len = 0u;
    return ESP_OK;
}

esp_err_t nvs_commit(nvs_handle_t handle)
{
    (void)handle;
    return ESP_OK;
}

// --- portal_auth device glue (issue #69) ----------------------------------
//
// The device build provides these two symbols from portal_auth_port.c
// (hardware RNG, mbedtls). The host build replaces them with deterministic
// implementations so the auth rules are testable: an advancing xorshift for
// the random source and a simple 32-byte mixing digest.

static uint32_t s_portal_rng_state = 0x1234abcdu;

void portal_auth_port_random(uint8_t *out, size_t len)
{
    for (size_t i = 0; i < len; ++i) {
        s_portal_rng_state ^= s_portal_rng_state << 13;
        s_portal_rng_state ^= s_portal_rng_state >> 17;
        s_portal_rng_state ^= s_portal_rng_state << 5;
        out[i] = (uint8_t)(s_portal_rng_state >> 24);
    }
}

esp_err_t portal_auth_port_sha256(const uint8_t *data, size_t len,
                                  uint8_t out[PORTAL_AUTH_DIGEST_LEN])
{
    uint32_t h[8];
    for (int r = 0; r < 8; ++r) {
        h[r] = 0x9e3779b9u ^ ((uint32_t)r * 0x85ebca6bu);
    }
    for (size_t i = 0; i < len; ++i) {
        uint32_t v = data[i];
        for (int r = 0; r < 8; ++r) {
            h[r] ^= v + 0x9e3779b9u + (h[r] << 6) + (h[r] >> 2);
            v = (v << 7) | (v >> 25);
        }
    }
    for (int r = 0; r < 8; ++r) {
        out[r * 4 + 0] = (uint8_t)(h[r]);
        out[r * 4 + 1] = (uint8_t)(h[r] >> 8);
        out[r * 4 + 2] = (uint8_t)(h[r] >> 16);
        out[r * 4 + 3] = (uint8_t)(h[r] >> 24);
    }
    return ESP_OK;
}
