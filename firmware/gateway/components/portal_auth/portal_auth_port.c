/**
 * @file portal_auth_port.c
 * @brief Device glue for portal_auth: hardware RNG and mbedtls SHA-256.
 *
 * The host test build does not compile this file; `tools/host-tests/shims`
 * provides deterministic implementations of the same two symbols.
 */

#include "portal_auth.h"

#include "esp_random.h"
#include "mbedtls/sha256.h"

void portal_auth_port_random(uint8_t *out, size_t len)
{
    esp_fill_random(out, len);
}

esp_err_t portal_auth_port_sha256(const uint8_t *data, size_t len,
                                  uint8_t out[PORTAL_AUTH_DIGEST_LEN])
{
    int ret = mbedtls_sha256(data, len, out, 0);
    return (ret == 0) ? ESP_OK : ESP_ERR_INVALID_STATE;
}
