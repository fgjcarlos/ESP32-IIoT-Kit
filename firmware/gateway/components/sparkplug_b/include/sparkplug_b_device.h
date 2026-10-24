#pragma once

// Device (Sparkplug Device) registry for the EoN-Node (issue #11, commit 8).
//
// Sparkplug B v3.0.0 models every ESP-NOW node behind the gateway as a
// Device. The registry keeps the per-device state the gateway needs to:
//   - know which devices exist and whether they are alive or stale,
//   - publish a DDEATH when a device stops reporting (expiry policy),
//   - republish the DBIRTH of every device when a rebirth is requested.
//
// Design reference: odd/tasks/issue-11-sparkplug-b/design.md §3.3.

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

/** Maximum number of tracked devices. Matches the ESP-NOW peer limit. */
#define SPARKPLUG_B_MAX_DEVICES 20

/** Maximum length of a device_id string ("N-" + 4 hex chars + NUL by default). */
#define SPARKPLUG_B_DEVICE_ID_MAX 32

/**
 * Maximum number of bytes kept per device for the last published payload.
 *
 * The stored bytes are replayed as the DBIRTH when a rebirth is requested, so
 * the bound has to cover a full device metric set. 20 devices x 384 bytes is
 * about 8 KiB of static memory, which is accounted for in
 * docs/sparkplug_3.0.0-measurements.md.
 */
#define SPARKPLUG_B_DEVICE_PAYLOAD_MAX 384

/** Lifecycle state of a Sparkplug Device. */
typedef enum {
    /** The device has been announced but has not reported yet. */
    SPARKPLUG_DEVICE_UNKNOWN = 0,
    /** The device reported recently and is considered online. */
    SPARKPLUG_DEVICE_ALIVE,
    /** The device stopped reporting and its DDEATH has been published. */
    SPARKPLUG_DEVICE_STALE,
} sparkplug_b_device_state_t;

/** Registry entry for one Sparkplug Device. */
typedef struct {
    char device_id[SPARKPLUG_B_DEVICE_ID_MAX];
    uint8_t mac[6];
    sparkplug_b_device_state_t state;
    uint64_t last_seen_ms;
    size_t payload_len;
    uint8_t payload[SPARKPLUG_B_DEVICE_PAYLOAD_MAX];
} sparkplug_b_device_t;

/** @brief Reset the registry. Clears every tracked device. */
esp_err_t sparkplug_b_devices_init(void);

/**
 * @brief Record that a device reported data.
 *
 * Marks the device ALIVE and refreshes its expiry deadline. Creates the entry
 * when the device is seen for the first time.
 *
 * @param device_id Sparkplug device id.
 * @param mac       MAC of the node behind the device (may be NULL).
 * @param now_ms    Current monotonic time in milliseconds.
 */
esp_err_t sparkplug_b_devices_touch(const char *device_id, const uint8_t mac[6],
                                    uint64_t now_ms);

/**
 * @brief Store the payload last published for a device.
 *
 * The bytes are replayed as the DBIRTH when a rebirth is requested. A payload
 * longer than SPARKPLUG_B_DEVICE_PAYLOAD_MAX is rejected so the registry
 * never truncates a payload into something a Host would misread.
 */
esp_err_t sparkplug_b_devices_store_payload(const char *device_id,
                                             const uint8_t *payload, size_t len);

/** @brief Copy the registry entry at `index` into `out`. */
esp_err_t sparkplug_b_devices_get(size_t index, sparkplug_b_device_t *out);

/** @return Number of tracked devices. */
size_t sparkplug_b_devices_count(void);

/**
 * @brief Find the index of a device id.
 *
 * @return ESP_OK and the index on success, ESP_ERR_NOT_FOUND when unknown.
 */
esp_err_t sparkplug_b_devices_find(const char *device_id, size_t *index);

/** @brief Update the lifecycle state of a device. */
esp_err_t sparkplug_b_devices_set_state(const char *device_id,
                                        sparkplug_b_device_state_t state);

/**
 * @brief Mark devices that stopped reporting as stale.
 *
 * A device is stale when it has not reported for `timeout_sec`. The indices
 * of the devices that transitioned from ALIVE to STALE are written to
 * `expired` so the caller can publish a DDEATH for each of them.
 *
 * @param now_ms       Current monotonic time in milliseconds.
 * @param timeout_sec  Silence that counts as a death.
 * @param expired      Destination array for the stale indices.
 * @param max_expired  Capacity of `expired`.
 * @return Number of devices that transitioned to STALE.
 */
size_t sparkplug_b_devices_expire(uint64_t now_ms, uint32_t timeout_sec,
                                  size_t *expired, size_t max_expired);

#ifdef __cplusplus
}
#endif
