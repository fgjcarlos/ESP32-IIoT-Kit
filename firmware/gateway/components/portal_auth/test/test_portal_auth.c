/**
 * @file test_portal_auth.c
 * @brief Unity tests for the portal credential and session rules (W02).
 *
 * Every stateful case starts from `portal_auth_reset` plus an explicit
 * provision, so the cases are order-independent. The device runner does not
 * initialize NVS itself, so each stateful case calls `ensure_nvs` first.
 */

#include <string.h>

#include "nvs.h"
#include "nvs_flash.h"
#include "portal_auth.h"
#include "unity.h"

static const char *TEST_CREDENTIAL = "acceso-inicial-123";

static void ensure_nvs(void)
{
    /* Idempotent: ESP_OK again when NVS is already initialized. */
    TEST_ASSERT_EQUAL(ESP_OK, nvs_flash_init());
}

static void fresh_provision(void)
{
    portal_auth_reset();
    TEST_ASSERT_EQUAL(ESP_OK, portal_auth_provision(TEST_CREDENTIAL));
}

static void erase_portal_keys(void)
{
    nvs_handle_t handle = 0;
    TEST_ASSERT_EQUAL(ESP_OK, nvs_open(PORTAL_AUTH_NVS_NAMESPACE, NVS_READWRITE,
                                       &handle));
    (void)nvs_erase_key(handle, PORTAL_AUTH_NVS_KEY_CRED_DIGEST);
    (void)nvs_erase_key(handle, PORTAL_AUTH_NVS_KEY_IS_INITIAL);
    TEST_ASSERT_EQUAL(ESP_OK, nvs_commit(handle));
    nvs_close(handle);
}

TEST_CASE("every NVS key of the portal namespace fits the NVS key length", "[portal_auth]")
{
#define CHECK_KEY_LENGTH(key) \
    TEST_ASSERT_LESS_OR_EQUAL_UINT32(15u, (uint32_t)strlen(key));
    PORTAL_AUTH_NVS_KEYS(CHECK_KEY_LENGTH)
#undef CHECK_KEY_LENGTH
    TEST_ASSERT_LESS_OR_EQUAL_UINT32(15u,
                                     (uint32_t)strlen(PORTAL_AUTH_NVS_NAMESPACE));
}

TEST_CASE("provision rejects NULL and out-of-range credentials", "[portal_auth]")
{
    char too_long[PORTAL_AUTH_CREDENTIAL_MAX_LEN + 2];
    memset(too_long, 'L', sizeof(too_long) - 1u);
    too_long[sizeof(too_long) - 1u] = '\0';

    TEST_ASSERT_EQUAL(ESP_ERR_INVALID_ARG, portal_auth_provision(NULL));
    TEST_ASSERT_EQUAL(ESP_ERR_INVALID_SIZE,
                      portal_auth_provision("corta"));
    TEST_ASSERT_EQUAL(ESP_ERR_INVALID_SIZE, portal_auth_provision(too_long));
}

TEST_CASE("login before init returns ESP_ERR_INVALID_STATE", "[portal_auth]")
{
    char token[PORTAL_AUTH_TOKEN_STR_LEN] = {0};

    portal_auth_reset();
    TEST_ASSERT_EQUAL(ESP_ERR_INVALID_STATE,
                      portal_auth_login(TEST_CREDENTIAL, token, sizeof(token)));
    TEST_ASSERT_EQUAL(ESP_ERR_INVALID_ARG,
                      portal_auth_login(NULL, token, sizeof(token)));
    TEST_ASSERT_EQUAL(ESP_ERR_INVALID_SIZE,
                      portal_auth_login("corta", token, sizeof(token)));
}

TEST_CASE("provision stores the credential and login accepts it", "[portal_auth]")
{
    char token[PORTAL_AUTH_TOKEN_STR_LEN] = {0};

    ensure_nvs();
    fresh_provision();

    TEST_ASSERT_TRUE(portal_auth_is_initial_credential());
    TEST_ASSERT_EQUAL(ESP_OK,
                      portal_auth_login(TEST_CREDENTIAL, token, sizeof(token)));
    TEST_ASSERT_EQUAL(PORTAL_AUTH_TOKEN_STR_LEN - 1u, strlen(token));
    TEST_ASSERT_TRUE(portal_auth_session_valid(token));
}

TEST_CASE("login with a wrong credential fails and keeps the active session", "[portal_auth]")
{
    char token[PORTAL_AUTH_TOKEN_STR_LEN] = {0};

    ensure_nvs();
    fresh_provision();
    TEST_ASSERT_EQUAL(ESP_OK,
                      portal_auth_login(TEST_CREDENTIAL, token, sizeof(token)));

    TEST_ASSERT_EQUAL(ESP_ERR_NOT_FOUND,
                      portal_auth_login("wrong-credential", token, sizeof(token)));
    TEST_ASSERT_TRUE(portal_auth_session_valid(token));
}

TEST_CASE("logout invalidates the session token", "[portal_auth]")
{
    char token[PORTAL_AUTH_TOKEN_STR_LEN] = {0};

    ensure_nvs();
    fresh_provision();
    TEST_ASSERT_EQUAL(ESP_OK,
                      portal_auth_login(TEST_CREDENTIAL, token, sizeof(token)));

    TEST_ASSERT_EQUAL(ESP_OK, portal_auth_logout(token));
    TEST_ASSERT_FALSE(portal_auth_session_valid(token));
    TEST_ASSERT_EQUAL(ESP_ERR_NOT_FOUND, portal_auth_logout(token));
}

TEST_CASE("a session token of the wrong length is never valid", "[portal_auth]")
{
    char token[PORTAL_AUTH_TOKEN_STR_LEN] = {0};

    ensure_nvs();
    fresh_provision();
    TEST_ASSERT_EQUAL(ESP_OK,
                      portal_auth_login(TEST_CREDENTIAL, token, sizeof(token)));

    /* Short, long and empty candidates must all be rejected without
     * comparing past the end of the candidate. */
    char corto[PORTAL_AUTH_TOKEN_STR_LEN] = {0};
    memcpy(corto, token, 8u);
    TEST_ASSERT_FALSE(portal_auth_session_valid(corto));
    TEST_ASSERT_FALSE(portal_auth_session_valid(""));
    TEST_ASSERT_FALSE(portal_auth_session_valid(NULL));
}

TEST_CASE("change requires the current credential", "[portal_auth]")
{
    char token[PORTAL_AUTH_TOKEN_STR_LEN] = {0};

    ensure_nvs();
    fresh_provision();
    TEST_ASSERT_EQUAL(ESP_OK,
                      portal_auth_login(TEST_CREDENTIAL, token, sizeof(token)));

    TEST_ASSERT_EQUAL(ESP_ERR_NOT_FOUND,
                      portal_auth_change_credential("wrong-current-1",
                                                    "nueva-clave-123",
                                                    "nueva-clave-123"));

    /* The stored credential is untouched. */
    TEST_ASSERT_TRUE(portal_auth_session_valid(token));
    TEST_ASSERT_EQUAL(ESP_OK,
                      portal_auth_login(TEST_CREDENTIAL, token, sizeof(token)));
}

TEST_CASE("change requires a matching repetition", "[portal_auth]")
{
    ensure_nvs();
    fresh_provision();

    TEST_ASSERT_EQUAL(ESP_ERR_INVALID_ARG,
                      portal_auth_change_credential(TEST_CREDENTIAL,
                                                    "nueva-clave-123",
                                                    "otra-cosa-999"));
    TEST_ASSERT_EQUAL(ESP_OK,
                      portal_auth_login(TEST_CREDENTIAL,
                                        (char[PORTAL_AUTH_TOKEN_STR_LEN]){0},
                                        PORTAL_AUTH_TOKEN_STR_LEN));
}

TEST_CASE("change enforces the credential length range", "[portal_auth]")
{
    char too_long[PORTAL_AUTH_CREDENTIAL_MAX_LEN + 2];
    memset(too_long, 'N', sizeof(too_long) - 1u);
    too_long[sizeof(too_long) - 1u] = '\0';

    ensure_nvs();
    fresh_provision();

    TEST_ASSERT_EQUAL(ESP_ERR_INVALID_SIZE,
                      portal_auth_change_credential(TEST_CREDENTIAL,
                                                    "corta", "corta"));
    TEST_ASSERT_EQUAL(ESP_ERR_INVALID_SIZE,
                      portal_auth_change_credential(TEST_CREDENTIAL,
                                                    too_long, too_long));
    TEST_ASSERT_EQUAL(ESP_OK,
                      portal_auth_login(TEST_CREDENTIAL,
                                        (char[PORTAL_AUTH_TOKEN_STR_LEN]){0},
                                        PORTAL_AUTH_TOKEN_STR_LEN));
}

TEST_CASE("successful change rotates the credential and clears the initial flag",
          "[portal_auth]")
{
    char token[PORTAL_AUTH_TOKEN_STR_LEN] = {0};

    ensure_nvs();
    fresh_provision();
    TEST_ASSERT_TRUE(portal_auth_is_initial_credential());

    TEST_ASSERT_EQUAL(ESP_OK,
                      portal_auth_change_credential(TEST_CREDENTIAL,
                                                    "nueva-clave-123",
                                                    "nueva-clave-123"));

    TEST_ASSERT_FALSE(portal_auth_is_initial_credential());
    TEST_ASSERT_EQUAL(ESP_ERR_NOT_FOUND,
                      portal_auth_login(TEST_CREDENTIAL, token, sizeof(token)));
    TEST_ASSERT_EQUAL(ESP_OK,
                      portal_auth_login("nueva-clave-123", token, sizeof(token)));
}

TEST_CASE("init with no stored credential provisions one", "[portal_auth]")
{
    ensure_nvs();
    erase_portal_keys();
    portal_auth_reset();

    TEST_ASSERT_EQUAL(ESP_OK, portal_auth_init());
    TEST_ASSERT_TRUE(portal_auth_is_initial_credential());
}

TEST_CASE("init with an existing credential keeps it", "[portal_auth]")
{
    char token[PORTAL_AUTH_TOKEN_STR_LEN] = {0};

    ensure_nvs();
    erase_portal_keys();
    portal_auth_reset();
    TEST_ASSERT_EQUAL(ESP_OK, portal_auth_provision("conocida-9876-a"));
    portal_auth_reset();

    TEST_ASSERT_EQUAL(ESP_OK, portal_auth_init());
    TEST_ASSERT_TRUE(portal_auth_is_initial_credential());
    TEST_ASSERT_EQUAL(ESP_OK,
                      portal_auth_login("conocida-9876-a", token, sizeof(token)));
}
