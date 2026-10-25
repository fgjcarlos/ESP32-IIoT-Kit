#pragma once

/**
 * @file ota_manager.h
 * @brief Secure over-the-air update of the gateway: fetch, verify, install.
 *
 * The manager is the ESP-IDF half of the OTA layer. The rules that decide
 * whether an image may be installed live in `ota_manifest` (portable, host
 * testable); this component executes them against the real device:
 *
 *   1. Fetch the release manifest over HTTPS.
 *   2. Parse it and verify its signature (ECDSA P-256 over the canonical
 *      form of the manifest).
 *   3. Check the compatibility of the image against the identity of this
 *      unit — product, brand, model, hardware revision and target chip —
 *      and the release channel of the unit.
 *   4. Only then download the image, write it into the inactive OTA slot
 *      and verify its SHA-256 against the digest the signed manifest
 *      declares.
 *   5. Point the bootloader at the new slot and reboot.
 *
 * Steps 1 to 3 happen **before a single byte of the image is fetched**, so a
 * foreign, unsigned or out-of-channel image costs one small request.
 *
 * A new image boots in the `PENDING_VERIFY` state. It only becomes
 * permanent when the application confirms it with
 * `ota_manager_confirm_pending_image()` once the critical subsystems are
 * alive. If the image crashes, the bootloader rolls back on the next boot.
 * If it hangs, the confirmation watchdog reboots the unit, and the
 * bootloader rolls back too.
 */

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "esp_err.h"
#include "ota_manifest.h"

#ifdef __cplusplus
extern "C" {
#endif

/** Length of a partition label, including the terminator. */
#define OTA_MANAGER_PARTITION_LABEL_LEN 17u

/** Length of the reported firmware version, including the terminator. */
#define OTA_MANAGER_VERSION_LEN 33u

/** Length of the reported release channel, including the terminator. */
#define OTA_MANAGER_CHANNEL_LEN 17u

/** Length of the reported rejection reason, including the terminator. */
#define OTA_MANAGER_REASON_LEN 40u

/** Length of the reported error name, including the terminator. */
#define OTA_MANAGER_ERROR_LEN 64u

/** Seconds the new image has to confirm itself before it is rolled back. */
#define OTA_MANAGER_CONFIRM_WINDOW_SEC 120

/**
 * @brief State of the OTA manager.
 */
typedef enum {
    OTA_STATE_IDLE = 0,        /**< Nothing is happening. */
    OTA_STATE_CHECKING,        /**< The manifest is being fetched and checked. */
    OTA_STATE_DOWNLOADING,     /**< The image is being written into the slot. */
    OTA_STATE_VERIFYING,       /**< The written image is being read back. */
    OTA_STATE_READY_TO_REBOOT, /**< The new image is installed and armed. */
    OTA_STATE_FAILED,          /**< The last attempt failed. */
} ota_state_t;

/**
 * @brief Snapshot of the OTA state of the unit.
 */
typedef struct {
    ota_state_t state;                              /**< Current state. */
    bool busy;                                     /**< An update is running. */
    bool pending_verify;                           /**< The running image is on trial. */
    char running_partition[OTA_MANAGER_PARTITION_LABEL_LEN]; /**< Slot that is running. */
    char boot_partition[OTA_MANAGER_PARTITION_LABEL_LEN];    /**< Slot the bootloader will pick. */
    char current_version[OTA_MANAGER_VERSION_LEN];  /**< Version of the running image. */
    char channel[OTA_MANAGER_CHANNEL_LEN];          /**< Release channel of this unit. */
    char last_reason[OTA_MANAGER_REASON_LEN];        /**< Reason of the last rejection. */
    char last_error[OTA_MANAGER_ERROR_LEN];          /**< Error of the last attempt. */
} ota_status_t;

/**
 * @brief Initialise the OTA manager.
 *
 * Reads the OTA state of the unit. When the running image is on trial — it
 * was installed by an update and has not confirmed itself yet — the
 * confirmation watchdog is armed.
 *
 * @return ESP_OK on success, otherwise an ESP-IDF error.
 */
esp_err_t ota_manager_init(void);

/**
 * @brief Start an update from an explicit manifest URL.
 *
 * The update runs in its own task, so the call returns as soon as it is
 * under way. Use `ota_manager_get_status()` to follow it.
 *
 * @param manifest_url HTTPS URL of the release manifest. May not be NULL.
 * @return ESP_OK when the update was started, ESP_ERR_INVALID_STATE when
 *         another update is already running.
 */
esp_err_t ota_manager_start_update(const char *manifest_url);

/**
 * @brief Start an update from the manifest URL stored in NVS.
 *
 * @return ESP_OK when the update was started, ESP_ERR_NOT_FOUND when no
 *         URL has been provisioned.
 */
esp_err_t ota_manager_start_configured_update(void);

/**
 * @brief Verify a manifest against the release key embedded in the firmware.
 *
 * The signature covers the canonical form of the manifest. The check is
 * exposed on its own so the release path can be audited — from the API, or
 * from a test — without starting an update.
 *
 * @param manifest Manifest to verify. May not be NULL.
 * @return ESP_OK when the signature is valid, ESP_ERR_INVALID_STATE when
 *         it is not.
 */
esp_err_t ota_manager_verify_manifest(const ota_manifest_t *manifest);

/**
 * @brief Report whether an update is running.
 */
bool ota_manager_is_busy(void);

/**
 * @brief Read a snapshot of the OTA state.
 *
 * @param out Destination. May not be NULL.
 * @return ESP_OK on success, ESP_ERR_INVALID_ARG for a NULL pointer.
 */
esp_err_t ota_manager_get_status(ota_status_t *out);

/**
 * @brief Confirm the running image and cancel the pending rollback.
 *
 * Call this once the critical subsystems are alive — the Wi-Fi is up and
 * the radio works. Until then the image stays on trial and the bootloader
 * rolls it back if the unit reboots.
 *
 * @return ESP_OK when the image was confirmed, ESP_ERR_INVALID_STATE when
 *         the running image was not on trial.
 */
esp_err_t ota_manager_confirm_pending_image(void);

/**
 * @brief Report whether the running image is on trial.
 */
bool ota_manager_image_pending(void);

/**
 * @brief Read the provisioned manifest URL.
 *
 * @param out     Destination buffer.
 * @param out_len Size of @p out.
 * @return ESP_OK on success, ESP_ERR_NOT_FOUND when no URL is provisioned.
 */
esp_err_t ota_manager_get_manifest_url(char *out, size_t out_len);

/**
 * @brief Store the manifest URL of the release server.
 *
 * The URL is stored in the "ota" namespace of NVS and survives a reboot.
 *
 * @param url HTTPS URL of the release manifest. May not be NULL.
 * @return ESP_OK on success.
 */
esp_err_t ota_manager_set_manifest_url(const char *url);

#ifdef __cplusplus
}
#endif
