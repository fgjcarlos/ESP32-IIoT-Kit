/**
 * @file portal_auth.c
 * @brief Single-owner credential and session for the local portal (W02).
 *
 * The credential is persisted only as its SHA-256 digest in the "portal" NVS
 * namespace (ADR-006). Comparison over digests is constant-time. The session
 * token lives in RAM only; a new login replaces it.
 */

#include "portal_auth.h"

#include <stdio.h>
#include <string.h>

#include "esp_log.h"
#include "nvs.h"
#include "nvs_flash.h"

static const char *TAG = "portal_auth";

static bool s_ready = false;
static bool s_is_initial = false;
static uint8_t s_digest[PORTAL_AUTH_DIGEST_LEN] = {0};
static bool s_session_active = false;
static char s_session_token[PORTAL_AUTH_TOKEN_STR_LEN] = {0};

static bool credential_len_in_range(const char *credential)
{
    size_t len = strnlen(credential, PORTAL_AUTH_CREDENTIAL_MAX_LEN + 1u);
    return (len >= PORTAL_AUTH_CREDENTIAL_MIN_LEN) &&
           (len <= PORTAL_AUTH_CREDENTIAL_MAX_LEN);
}

static bool ct_equal(const uint8_t *a, const uint8_t *b, size_t len)
{
    uint8_t diff = 0u;
    for (size_t i = 0; i < len; ++i) {
        diff |= (uint8_t)(a[i] ^ b[i]);
    }
    return diff == 0u;
}

static esp_err_t digest_of(const char *credential, uint8_t out[PORTAL_AUTH_DIGEST_LEN])
{
    return portal_auth_port_sha256((const uint8_t *)credential,
                                   strlen(credential), out);
}

static esp_err_t persist_credential(const char *credential, uint8_t initial_flag)
{
    uint8_t digest[PORTAL_AUTH_DIGEST_LEN];
    esp_err_t err = digest_of(credential, digest);
    if (err != ESP_OK) {
        return err;
    }

    nvs_handle_t handle = 0;
    err = nvs_open(PORTAL_AUTH_NVS_NAMESPACE, NVS_READWRITE, &handle);
    if (err != ESP_OK) {
        return err;
    }

    err = nvs_set_blob(handle, PORTAL_AUTH_NVS_KEY_CRED_DIGEST, digest,
                       sizeof(digest));
    if (err == ESP_OK) {
        err = nvs_set_u8(handle, PORTAL_AUTH_NVS_KEY_IS_INITIAL, initial_flag);
    }
    if (err == ESP_OK) {
        err = nvs_commit(handle);
    }
    nvs_close(handle);

    if (err == ESP_OK) {
        memcpy(s_digest, digest, sizeof(s_digest));
        s_is_initial = (initial_flag != 0u);
        s_ready = true;
    }
    return err;
}

esp_err_t portal_auth_init(void)
{
    if (s_ready) {
        return ESP_OK;
    }

    nvs_handle_t handle = 0;
    esp_err_t err = nvs_open(PORTAL_AUTH_NVS_NAMESPACE, NVS_READWRITE, &handle);
    if (err != ESP_OK) {
        return err;
    }

    uint8_t digest[PORTAL_AUTH_DIGEST_LEN] = {0};
    size_t len = sizeof(digest);
    err = nvs_get_blob(handle, PORTAL_AUTH_NVS_KEY_CRED_DIGEST, digest, &len);
    if (err == ESP_ERR_NVS_NOT_FOUND) {
        nvs_close(handle);

        /* First boot: provision a random credential and reveal it once on
         * the serial console (ADR-006). Only the digest is persisted. */
        uint8_t raw[8] = {0};
        char credential[2 * sizeof(raw) + 1u] = {0};
        portal_auth_port_random(raw, sizeof(raw));
        for (size_t i = 0; i < sizeof(raw); ++i) {
            snprintf(&credential[i * 2u], 3u, "%02x", raw[i]);
        }
        ESP_LOGI(TAG,
                 "portal initial credential: %.4s-%.4s-%.4s-%.4s "
                 "(shown once; change it after the first login)",
                 credential, &credential[4], &credential[8], &credential[12]);
        return persist_credential(credential, 1u);
    }
    if (err != ESP_OK) {
        nvs_close(handle);
        return err;
    }

    uint8_t initial = 0u;
    err = nvs_get_u8(handle, PORTAL_AUTH_NVS_KEY_IS_INITIAL, &initial);
    nvs_close(handle);
    if (err != ESP_OK) {
        return err;
    }

    memcpy(s_digest, digest, sizeof(s_digest));
    s_is_initial = (initial != 0u);
    s_ready = true;
    return ESP_OK;
}

void portal_auth_reset(void)
{
    s_ready = false;
    s_is_initial = false;
    memset(s_digest, 0, sizeof(s_digest));
    s_session_active = false;
    memset(s_session_token, 0, sizeof(s_session_token));
}

esp_err_t portal_auth_provision(const char *credential)
{
    if (credential == NULL) {
        return ESP_ERR_INVALID_ARG;
    }
    if (!credential_len_in_range(credential)) {
        return ESP_ERR_INVALID_SIZE;
    }
    return persist_credential(credential, 1u);
}

bool portal_auth_is_initial_credential(void)
{
    return s_is_initial;
}

esp_err_t portal_auth_login(const char *credential, char *token_out,
                            size_t token_len)
{
    if (credential == NULL || token_out == NULL ||
        token_len < PORTAL_AUTH_TOKEN_STR_LEN) {
        return ESP_ERR_INVALID_ARG;
    }
    if (!credential_len_in_range(credential)) {
        return ESP_ERR_INVALID_SIZE;
    }
    if (!s_ready) {
        return ESP_ERR_INVALID_STATE;
    }

    uint8_t digest[PORTAL_AUTH_DIGEST_LEN];
    esp_err_t err = digest_of(credential, digest);
    if (err != ESP_OK) {
        return err;
    }
    if (!ct_equal(digest, s_digest, sizeof(digest))) {
        return ESP_ERR_NOT_FOUND;
    }

    uint8_t raw[16] = {0};
    portal_auth_port_random(raw, sizeof(raw));
    for (size_t i = 0; i < sizeof(raw); ++i) {
        snprintf(&s_session_token[i * 2u], 3u, "%02x", raw[i]);
    }
    s_session_token[PORTAL_AUTH_TOKEN_STR_LEN - 1u] = '\0';
    s_session_active = true;

    memcpy(token_out, s_session_token, PORTAL_AUTH_TOKEN_STR_LEN);
    return ESP_OK;
}

bool portal_auth_session_valid(const char *token)
{
    if (token == NULL || !s_session_active) {
        return false;
    }
    /* Only tokens of the exact expected length are compared: otherwise the
     * constant-time walk would read past the end of a shorter candidate. */
    if (strnlen(token, PORTAL_AUTH_TOKEN_STR_LEN) !=
        (PORTAL_AUTH_TOKEN_STR_LEN - 1u)) {
        return false;
    }
    return ct_equal((const uint8_t *)token, (const uint8_t *)s_session_token,
                    PORTAL_AUTH_TOKEN_STR_LEN - 1u);
}

esp_err_t portal_auth_logout(const char *token)
{
    if (token == NULL) {
        return ESP_ERR_INVALID_ARG;
    }
    if (!portal_auth_session_valid(token)) {
        return ESP_ERR_NOT_FOUND;
    }
    s_session_active = false;
    memset(s_session_token, 0, sizeof(s_session_token));
    return ESP_OK;
}

esp_err_t portal_auth_change_credential(const char *current,
                                        const char *new_credential,
                                        const char *repeat)
{
    if (current == NULL || new_credential == NULL || repeat == NULL) {
        return ESP_ERR_INVALID_ARG;
    }
    if (!s_ready) {
        return ESP_ERR_INVALID_STATE;
    }

    /* Authorization first: a rejected change must not touch storage. */
    uint8_t digest[PORTAL_AUTH_DIGEST_LEN];
    esp_err_t err = digest_of(current, digest);
    if (err != ESP_OK) {
        return err;
    }
    if (!ct_equal(digest, s_digest, sizeof(digest))) {
        return ESP_ERR_NOT_FOUND;
    }
    if (strcmp(new_credential, repeat) != 0) {
        return ESP_ERR_INVALID_ARG;
    }
    if (!credential_len_in_range(new_credential)) {
        return ESP_ERR_INVALID_SIZE;
    }

    return persist_credential(new_credential, 0u);
}
