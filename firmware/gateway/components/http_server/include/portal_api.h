#pragma once

/**
 * @file portal_api.h
 * @brief Portable helpers of the local portal API (W02, issue #69).
 *
 * Everything here is host-testable: no esp_http_server, only the portable
 * wifi_manager types. The HTTP glue in http_server.c serializes its JSON
 * responses and reads its session cookie through this interface, so the
 * exact bytes on the wire are covered by Unity tests on the host and on
 * the device.
 */

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "esp_err.h"
#include "wifi_manager.h"

#ifdef __cplusplus
extern "C" {
#endif

/** Name of the portal session cookie (HttpOnly; Path=/). */
#define PORTAL_API_SESSION_COOKIE_NAME "portal_session"

/**
 * @brief Maximum length of the status JSON, including the terminator.
 *
 * Sized for the worst case: both SSIDs at the 802.11 cap, both IPs at the
 * IPv4 cap, the full MAC and a 20-digit uptime. The worst-case test pins
 * that a buffer of this size always suffices.
 */
/**
 * @brief Maximum length of the status document.
 *
 * Sized for the worst case: every field of the status holds characters that
 * have to be escaped in JSON (a control character costs six bytes).
 */
#define PORTAL_API_STATUS_JSON_MAX_LEN 1024u

/**
 * @brief Serialize the document served by GET /api/status.
 *
 * The output is compact (no spaces) and deterministic:
 * `{"ap":{"up":..,"ssid":"..","ip":"..","channel":..,"mac":".."},`
 * `"sta":{"state":"..","ssid":"..","ip":".."},`
 * `"uptime_s":..,"portal":{"initial_credential":..}}` with sta.state in
 * idle|connecting|connected|disconnected.
 *
 * @param status             Snapshot from wifi_manager_get_status.
 * @param uptime_s           Seconds since boot (esp_timer_get_time()/1000000).
 * @param initial_credential portal_auth_is_initial_credential() at call time.
 * @param out                Destination buffer.
 * @param out_len            Size of @p out.
 * @return ESP_OK on success, ESP_ERR_INVALID_ARG on a NULL pointer or an
 *         unknown sta_state, ESP_ERR_INVALID_SIZE when the buffer is too
 *         small.
 */
esp_err_t portal_api_status_json(const wifi_manager_status_t *status,
                                 uint64_t uptime_s, bool initial_credential,
                                 char *out, size_t out_len);

/**
 * @brief Serialize the JSON error body of an API rejection.
 *
 * @param error_key Stable error identifier the SPA renders ("no-autorizado",
 *                  "credencial-incorrecta", ...).
 * @param out       Destination buffer.
 * @param out_len   Size of @p out.
 * @return ESP_OK on success, ESP_ERR_INVALID_ARG on a NULL pointer,
 *         ESP_ERR_INVALID_SIZE when the buffer is too small.
 */
esp_err_t portal_api_error_json(const char *error_key, char *out, size_t out_len);

/**
 * @brief Extract the portal session token from a Cookie header value.
 *
 * Walks the `name=value` pairs of @p cookie_header (separated by `;`) and
 * copies the value of PORTAL_API_SESSION_COOKIE_NAME.
 *
 * @param cookie_header Raw Cookie header value (may hold several pairs).
 * @param out           Destination buffer for the NUL-terminated value.
 * @param out_len       Size of @p out.
 * @return ESP_OK on success, ESP_ERR_INVALID_ARG on a NULL pointer,
 *         ESP_ERR_NOT_FOUND when the cookie is absent,
 *         ESP_ERR_INVALID_SIZE when @p out cannot hold the value.
 */
esp_err_t portal_api_cookie_value(const char *cookie_header, char *out,
                                  size_t out_len);

#ifdef __cplusplus
}
#endif
