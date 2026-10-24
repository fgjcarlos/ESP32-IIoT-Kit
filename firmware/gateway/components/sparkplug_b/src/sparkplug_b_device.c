#include "sparkplug_b_device.h"

#include <string.h>

#include "esp_log.h"

#include "sparkplug_b_device.h"

#define TAG "sparkplug_b_device"

// The registry is a fixed-size table: the ESP-NOW peer limit bounds the
// number of devices the gateway can ever talk to, so a static table keeps
// the module free of heap churn and of any allocation failure path.
static sparkplug_b_device_t s_devices[SPARKPLUG_B_MAX_DEVICES];
static size_t s_count;

static size_t find_index(const char *device_id)
{
    for (size_t i = 0; i < s_count; ++i) {
        if (strncmp(s_devices[i].device_id, device_id, SPARKPLUG_B_DEVICE_ID_MAX) == 0) {
            return i;
        }
    }
    return SPARKPLUG_B_MAX_DEVICES;
}

esp_err_t sparkplug_b_devices_init(void)
{
    memset(s_devices, 0, sizeof(s_devices));
    s_count = 0;
    return ESP_OK;
}

esp_err_t sparkplug_b_devices_touch(const char *device_id, const uint8_t mac[6],
                                    uint64_t now_ms)
{
    if (device_id == NULL || device_id[0] == '\0') {
        return ESP_ERR_INVALID_ARG;
    }

    size_t index = find_index(device_id);
    if (index == SPARKPLUG_B_MAX_DEVICES) {
        if (s_count >= SPARKPLUG_B_MAX_DEVICES) {
            ESP_LOGW(TAG, "Registry full, ignoring device %s", device_id);
            return ESP_ERR_NO_MEM;
        }
        index = s_count++;
        memset(&s_devices[index], 0, sizeof(s_devices[index]));
        snprintf(s_devices[index].device_id, SPARKPLUG_B_DEVICE_ID_MAX, "%s", device_id);
        ESP_LOGI(TAG, "Registered device %s", device_id);
    }

    if (mac != NULL) {
        memcpy(s_devices[index].mac, mac, sizeof(s_devices[index].mac));
    }
    s_devices[index].state = SPARKPLUG_DEVICE_ALIVE;
    s_devices[index].last_seen_ms = now_ms;
    return ESP_OK;
}

esp_err_t sparkplug_b_devices_store_payload(const char *device_id,
                                             const uint8_t *payload, size_t len)
{
    if (device_id == NULL || payload == NULL) {
        return ESP_ERR_INVALID_ARG;
    }
    if (len > SPARKPLUG_B_DEVICE_PAYLOAD_MAX) {
        // Truncating would produce a payload a Host would misread, so the
        // device is stored without a replayable payload instead.
        ESP_LOGW(TAG, "Payload of %zu bytes for %s exceeds the %d byte bound",
                 len, device_id, (int)SPARKPLUG_B_DEVICE_PAYLOAD_MAX);
        return ESP_ERR_INVALID_SIZE;
    }

    size_t index = find_index(device_id);
    if (index == SPARKPLUG_B_MAX_DEVICES) {
        return ESP_ERR_NOT_FOUND;
    }
    memcpy(s_devices[index].payload, payload, len);
    s_devices[index].payload_len = len;
    return ESP_OK;
}

esp_err_t sparkplug_b_devices_get(size_t index, sparkplug_b_device_t *out)
{
    if (out == NULL) {
        return ESP_ERR_INVALID_ARG;
    }
    if (index >= s_count) {
        return ESP_ERR_NOT_FOUND;
    }
    *out = s_devices[index];
    return ESP_OK;
}

size_t sparkplug_b_devices_count(void)
{
    return s_count;
}

esp_err_t sparkplug_b_devices_find(const char *device_id, size_t *index)
{
    if (device_id == NULL || index == NULL) {
        return ESP_ERR_INVALID_ARG;
    }
    size_t found = find_index(device_id);
    if (found == SPARKPLUG_B_MAX_DEVICES) {
        return ESP_ERR_NOT_FOUND;
    }
    *index = found;
    return ESP_OK;
}

esp_err_t sparkplug_b_devices_set_state(const char *device_id,
                                        sparkplug_b_device_state_t state)
{
    if (device_id == NULL) {
        return ESP_ERR_INVALID_ARG;
    }
    size_t index = find_index(device_id);
    if (index == SPARKPLUG_B_MAX_DEVICES) {
        return ESP_ERR_NOT_FOUND;
    }
    s_devices[index].state = state;
    return ESP_OK;
}

size_t sparkplug_b_devices_expire(uint64_t now_ms, uint32_t timeout_sec,
                                  size_t *expired, size_t max_expired)
{
    if (timeout_sec == 0) {
        return 0;
    }
    uint64_t timeout_ms = (uint64_t)timeout_sec * 1000ULL;
    size_t marked = 0;

    for (size_t i = 0; i < s_count; ++i) {
        if (s_devices[i].state != SPARKPLUG_DEVICE_ALIVE) {
            continue;
        }
        if (now_ms - s_devices[i].last_seen_ms < timeout_ms) {
            continue;
        }
        s_devices[i].state = SPARKPLUG_DEVICE_STALE;
        ESP_LOGW(TAG, "Device %s timed out after %u s without data",
                 s_devices[i].device_id, (unsigned int)timeout_sec);
        if (expired != NULL && marked < max_expired) {
            expired[marked] = i;
        }
        marked++;
    }
    return marked;
}
