#pragma once

/**
 * @file portal_auth.h
 * @brief Single-owner credential and session for the local portal (W02, issue #69).
 *
 * First provisioning (ADR-006, `knowledge/Decisiones/006-portal-provision-inicial.md`):
 * `portal_auth_init` generates a random credential when none is stored,
 * reveals it once through the serial log and persists only its SHA-256
 * digest in the "portal" NVS namespace. The interface prompts a change while
 * the initial credential is in use. If the credential is lost, recovery is
 * physical: erase the NVS partition and let the portal provision again.
 *
 * Not thread-safe: the portal is single-owner, so all calls come from the
 * HTTP server task.
 */

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

/** Minimum credential length (the generated initial credential is 16 chars). */
#define PORTAL_AUTH_CREDENTIAL_MIN_LEN 8u
/** Maximum credential length, aligned with the WPA2 passphrase cap. */
#define PORTAL_AUTH_CREDENTIAL_MAX_LEN 63u
/** SHA-256 digest length in bytes. */
#define PORTAL_AUTH_DIGEST_LEN 32u
/** Session token: 32 lowercase hex characters + NUL. */
#define PORTAL_AUTH_TOKEN_STR_LEN 33u

/** NVS namespace that owns the portal credential. */
#define PORTAL_AUTH_NVS_NAMESPACE "portal"

/** Digest of the stored credential (blob, PORTAL_AUTH_DIGEST_LEN bytes). */
#define PORTAL_AUTH_NVS_KEY_CRED_DIGEST "cred_digest"
/** 1 while the initial provisioned credential is still in use. */
#define PORTAL_AUTH_NVS_KEY_IS_INITIAL "is_initial"

/** NVS keys of the "portal" namespace (NVS caps keys at 15 characters). */
#define PORTAL_AUTH_NVS_KEYS(X)    \
    X(PORTAL_AUTH_NVS_KEY_CRED_DIGEST) \
    X(PORTAL_AUTH_NVS_KEY_IS_INITIAL)

/**
 * @brief Fill `out` with `len` random bytes (device: hardware RNG).
 */
void portal_auth_port_random(uint8_t *out, size_t len);

/**
 * @brief Compute the SHA-256 digest of `data`.
 * @return ESP_OK, otherwise the crypto error.
 */
esp_err_t portal_auth_port_sha256(const uint8_t *data, size_t len,
                                  uint8_t out[PORTAL_AUTH_DIGEST_LEN]);

/**
 * @brief Initialize the portal credential store and the runtime state.
 *
 * When no credential is stored, provisions one: 8 random bytes rendered as a
 * 16-character hex credential, revealed once with ESP_LOGI, persisted as its
 * digest.
 *
 * @return ESP_OK, otherwise an NVS or port error.
 */
esp_err_t portal_auth_init(void);

/**
 * @brief Clear the in-memory session state (test/teardown helper).
 */
void portal_auth_reset(void);

/**
 * @brief Provision an explicit credential (tests and factory path).
 *
 * @return ESP_OK; ESP_ERR_INVALID_ARG on NULL;
 *         ESP_ERR_INVALID_SIZE outside [MIN_LEN, MAX_LEN].
 */
esp_err_t portal_auth_provision(const char *credential);

/**
 * @brief True while the initial (provisioned) credential is still in use.
 */
bool portal_auth_is_initial_credential(void);

/**
 * @brief Verify a credential and mint the single session token.
 *
 * A successful login replaces any previous token.
 *
 * @return ESP_OK; ESP_ERR_NOT_FOUND on a wrong credential;
 *         ESP_ERR_INVALID_STATE before portal_auth_init;
 *         ESP_ERR_INVALID_ARG on NULL buffers or a short `token_out`;
 *         ESP_ERR_INVALID_SIZE when the candidate is out of range.
 */
esp_err_t portal_auth_login(const char *credential, char *token_out,
                            size_t token_len);

/**
 * @brief True when `token` is the active session token.
 */
bool portal_auth_session_valid(const char *token);

/**
 * @brief Invalidate the active session when `token` matches it.
 *
 * @return ESP_OK; ESP_ERR_NOT_FOUND when the token is not the active
 *         session; ESP_ERR_INVALID_ARG on NULL.
 */
esp_err_t portal_auth_logout(const char *token);

/**
 * @brief Change the stored credential.
 *
 * Requires the current credential and a matching repetition. A rejected
 * change leaves the stored credential untouched. A successful change clears
 * the initial-credential flag and keeps the active session.
 *
 * @return ESP_OK; ESP_ERR_NOT_FOUND when `current` is wrong;
 *         ESP_ERR_INVALID_ARG on NULL pointers or mismatched repetition;
 *         ESP_ERR_INVALID_SIZE when a credential is out of range;
 *         ESP_ERR_INVALID_STATE before portal_auth_init.
 */
esp_err_t portal_auth_change_credential(const char *current,
                                        const char *new_credential,
                                        const char *repeat);

#ifdef __cplusplus
}
#endif
