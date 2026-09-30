#pragma once

/**
 * @file wifi_manager.h
 * @brief AP+STA lifecycle for the ESP32-S3 IIoT gateway (Phase 0).
 *
 * Exposes deterministic helpers used by the gateway and by Unity tests.
 * The implementation lives in `wifi_manager.c` and is built into the gateway
 * app and into the on-target test runner via `TEST_COMPONENTS`.
 *
 * Credentials are passed in by the caller (typically `main.c`) so that
 * secrets stay in the gitignored `wifi_credentials.local.h` header.
 */

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

/** Maximum SSID octets per IEEE 802.11 (32). */
#define WIFI_MANAGER_SSID_MAX_LEN         32u
/** Maximum WPA2-PSK passphrase length (63 printable ASCII + NUL). */
#define WIFI_MANAGER_PASSWORD_MAX_LEN     64u
/** Rendered MAC length: 6 * 2 hex + 5 colons + NUL = 18 bytes. */
#define WIFI_MANAGER_MAC_STR_LEN          18u
/** Lowest 2.4 GHz channel allowed for the soft-AP. */
#define WIFI_MANAGER_AP_CHANNEL_MIN       1u
/** Highest 2.4 GHz channel allowed for the soft-AP. */
#define WIFI_MANAGER_AP_CHANNEL_MAX       13u
/** Default AP SSID advertised in logs and produced by the default config. */
#define WIFI_MANAGER_AP_DEFAULT_SSID      "IIoT-Gateway"
/** Default channel selected when the caller leaves `ap_channel` at zero. */
#define WIFI_MANAGER_AP_DEFAULT_CHANNEL   1u
/** Default maximum simultaneous AP clients. */
#define WIFI_MANAGER_AP_DEFAULT_MAX_CONN  4u

/**
 * @brief Caller-supplied AP+STA configuration.
 *
 * The strings are read-only and must remain valid for the duration of
 * `wifi_manager_init`. Any pointer may be `NULL` to keep the related
 * default; helper functions below also validate ranges.
 */
typedef struct {
    const char *sta_ssid;          /**< NULL disables STA association. */
    const char *sta_password;      /**< NULL/empty means open STA. */
    const char *ap_ssid;           /**< NULL uses WIFI_MANAGER_AP_DEFAULT_SSID. */
    const char *ap_password;       /**< NULL/empty uses the AP open fallback. */
    uint8_t     ap_channel;        /**< 0 uses WIFI_MANAGER_AP_DEFAULT_CHANNEL. */
    uint8_t     ap_max_connection; /**< 0 uses WIFI_MANAGER_AP_DEFAULT_MAX_CONN. */
} wifi_manager_credentials_t;

/**
 * @brief Bring up the Wi-Fi driver in AP+STA mode.
 *
 * Performs NVS init (if not already done), netif init, default event loop,
 * AP/STA netif creation, Wi-Fi driver init, mode set, config apply, and
 * start. Logs every event of interest.
 *
 * @return ESP_OK on success, otherwise an ESP-IDF error.
 */
esp_err_t wifi_manager_init(const wifi_manager_credentials_t *credentials);

/**
 * @brief Stop Wi-Fi, free driver and netif resources.
 */
esp_err_t wifi_manager_deinit(void);

/**
 * @brief Validate that a 2.4 GHz channel is in [1, 13].
 */
esp_err_t wifi_manager_validate_ap_channel(uint8_t channel, bool *is_valid);

/**
 * @brief Validate that a SSID is non-NULL and within the 802.11 length cap.
 */
esp_err_t wifi_manager_ssid_in_range(const char *ssid);

/**
 * @brief Validate that a password is non-NULL and within the WPA2 cap.
 *
 * Empty passwords are allowed to model open networks.
 */
esp_err_t wifi_manager_password_in_range(const char *password);

/**
 * @brief Format a 6-octet MAC address as `aa:bb:cc:dd:ee:ff` (lowercase).
 *
 * @param mac       Input 6-byte buffer (read-only).
 * @param out       Output buffer.
 * @param out_len   Must be at least WIFI_MANAGER_MAC_STR_LEN bytes.
 */
esp_err_t wifi_manager_format_mac(const uint8_t mac[6], char *out, size_t out_len);

/**
 * @brief Resolve effective credentials, applying defaults for zero/NULL fields.
 *
 * Copies the input and replaces empty SSID/channel/max_connection with the
 * gateway defaults. The caller owns the destination buffer.
 */
esp_err_t wifi_manager_apply_credentials(const wifi_manager_credentials_t *in,
                                         wifi_manager_credentials_t *out);

#ifdef __cplusplus
}
#endif
