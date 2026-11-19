#pragma once

#include <stddef.h>

#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Maximum length of the brand payload, including the terminator.
 *
 * The payload is deterministic, so the size only depends on the brand
 * configuration. The value leaves room for a long support URL and a long
 * product name.
 */
#define HTTP_SERVER_BRAND_PAYLOAD_MAX_LEN 768u

/**
 * @brief Start the local portal HTTP server (W02, issue #69).
 *
 * Initializes the portal credential store (`portal_auth_init`) and serves
 * the embedded portal assets and API over esp_http_server.
 *
 * @return ESP_OK on success, ESP_ERR_INVALID_STATE when already running,
 *         otherwise the esp_http_server / portal_auth error.
 */
esp_err_t http_server_start(void);

/**
 * @brief Stop the portal HTTP server and reset its state.
 *
 * @return ESP_OK on success, ESP_ERR_INVALID_STATE when not running,
 *         otherwise the httpd error.
 */
esp_err_t http_server_stop(void);

/**
 * @brief Build the JSON payload of the brand endpoint.
 *
 * Issue #12: the identity of the product — brand name, model, version,
 * revision, OTA channel, support URL, initial SSID, colours and logo — must
 * be reachable from the API. This function produces the exact bytes the
 * `/api/brand` endpoint serves, so the API and the device can never report
 * different identities.
 *
 * The payload is deterministic: the same profile always yields byte-identical
 * JSON.
 *
 * @param out     Destination buffer.
 * @param out_len Size of @p out.
 * @return ESP_OK on success, ESP_ERR_INVALID_ARG on a NULL pointer,
 *         ESP_ERR_INVALID_SIZE when the buffer is too small.
 */
esp_err_t http_server_brand_payload(char *out, size_t out_len);

#ifdef __cplusplus
}
#endif
