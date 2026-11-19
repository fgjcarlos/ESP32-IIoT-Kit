/**
 * @file test_portal_api.c
 * @brief Unity tests for the portable portal API helpers (W02, issue #69).
 *
 * Runs on the host and on the device: only wifi_manager types and the
 * portal_api serialization are involved, no esp_http_server.
 */

#include <stdio.h>
#include <string.h>

#include "portal_api.h"
#include "unity.h"
#include "wifi_manager.h"

static wifi_manager_status_t fixed_status(void)
{
    wifi_manager_status_t status = {0};
    status.ap_up = true;
    strcpy(status.ap_ssid, "IIoT-Gateway");
    status.ap_channel = 1u;
    strcpy(status.ap_ip, "192.168.4.1");
    strcpy(status.ap_mac, "aa:bb:cc:dd:ee:ff");
    status.sta_state = WIFI_MANAGER_STA_IDLE;
    status.sta_ssid[0] = '\0';
    status.sta_ip[0] = '\0';
    return status;
}

TEST_CASE("status json matches the API contract byte for byte", "[portal_api]")
{
    char out[PORTAL_API_STATUS_JSON_MAX_LEN] = {0};
    const wifi_manager_status_t status = fixed_status();

    TEST_ASSERT_EQUAL(ESP_OK,
                      portal_api_status_json(&status, 123u, true, out, sizeof(out)));
    TEST_ASSERT_EQUAL_STRING(
        "{\"ap\":{\"up\":true,\"ssid\":\"IIoT-Gateway\",\"ip\":\"192.168.4.1\","
        "\"channel\":1,\"mac\":\"aa:bb:cc:dd:ee:ff\"},"
        "\"sta\":{\"state\":\"idle\",\"ssid\":\"\",\"ip\":\"\"},"
        "\"uptime_s\":123,\"portal\":{\"initial_credential\":true}}",
        out);
}

TEST_CASE("status json names every sta state", "[portal_api]")
{
    const struct {
        wifi_manager_sta_state_t state;
        const char *name;
    } cases[] = {
        {WIFI_MANAGER_STA_IDLE, "idle"},
        {WIFI_MANAGER_STA_CONNECTING, "connecting"},
        {WIFI_MANAGER_STA_CONNECTED, "connected"},
        {WIFI_MANAGER_STA_DISCONNECTED, "disconnected"},
    };

    for (size_t i = 0; i < sizeof(cases) / sizeof(cases[0]); ++i) {
        wifi_manager_status_t status = fixed_status();
        status.ap_up = false;
        status.sta_state = cases[i].state;
        strcpy(status.sta_ssid, "planta-baja");
        strcpy(status.sta_ip, "10.0.0.5");

        char out[PORTAL_API_STATUS_JSON_MAX_LEN] = {0};
        char expected[40] = {0};
        snprintf(expected, sizeof(expected), "\"state\":\"%s\"", cases[i].name);

        TEST_ASSERT_EQUAL(ESP_OK, portal_api_status_json(&status, 7u, false, out,
                                                         sizeof(out)));
        TEST_ASSERT_NOT_NULL(strstr(out, expected));
        TEST_ASSERT_NOT_NULL(strstr(out, "\"up\":false"));
        TEST_ASSERT_NOT_NULL(strstr(out, "\"initial_credential\":false"));
    }
}

TEST_CASE("status json rejects truncation and NULL pointers", "[portal_api]")
{
    const wifi_manager_status_t status = fixed_status();
    char small[32] = {0};

    TEST_ASSERT_EQUAL(ESP_ERR_INVALID_SIZE,
                      portal_api_status_json(&status, 123u, true, small, sizeof(small)));
    TEST_ASSERT_EQUAL(ESP_ERR_INVALID_ARG,
                      portal_api_status_json(NULL, 123u, true, small, sizeof(small)));
    TEST_ASSERT_EQUAL(ESP_ERR_INVALID_ARG,
                      portal_api_status_json(&status, 123u, true, NULL, sizeof(small)));
}

TEST_CASE("worst-case status json fits the declared maximum", "[portal_api]")
{
    wifi_manager_status_t status = {0};
    status.ap_up = true;
    status.ap_channel = 13u;
    memset(status.ap_ssid, 'S', WIFI_MANAGER_SSID_MAX_LEN);
    memset(status.sta_ssid, 'T', WIFI_MANAGER_SSID_MAX_LEN);
    memset(status.ap_ip, '1', WIFI_MANAGER_IPV4_STR_LEN - 1u);
    memset(status.sta_ip, '2', WIFI_MANAGER_IPV4_STR_LEN - 1u);
    strcpy(status.ap_mac, "aa:bb:cc:dd:ee:ff");
    status.sta_state = WIFI_MANAGER_STA_DISCONNECTED;

    char out[PORTAL_API_STATUS_JSON_MAX_LEN] = {0};
    TEST_ASSERT_EQUAL(ESP_OK, portal_api_status_json(&status, UINT64_MAX, true, out,
                                                     sizeof(out)));
}

TEST_CASE("status json escapes the characters that would break the document",
          "[portal_api]")
{
    wifi_manager_status_t status = fixed_status();
    strcpy(status.ap_ssid, "Cafe\"del\\nodo");
    status.sta_state = WIFI_MANAGER_STA_CONNECTED;

    char out[PORTAL_API_STATUS_JSON_MAX_LEN] = {0};
    TEST_ASSERT_EQUAL(ESP_OK,
                      portal_api_status_json(&status, 7u, false, out, sizeof(out)));

    /* The escaped form is what a JSON parser has to read. */
    TEST_ASSERT_NOT_NULL(strstr(out, "Cafe\\\"del\\\\nodo"));
    /* Every quote that opens or closes a string is unescaped, so the number
     * of unescaped quotes is even when the payload is escaped. */
    size_t quotes = 0u;
    for (const char *p = out; *p != '\0'; ++p) {
        if (*p == '\\' && *(p + 1) != '\0') {
            ++p; /* skip the escaped character */
            continue;
        }
        if (*p == '"') {
            ++quotes;
        }
    }
    TEST_ASSERT_EQUAL_UINT32(0u, quotes % 2u);
}

TEST_CASE("cookie parser finds the session token among the pairs", "[portal_api]")
{
    char out[64];

    TEST_ASSERT_EQUAL(ESP_OK,
                      portal_api_cookie_value("portal_session=abc123", out, sizeof(out)));
    TEST_ASSERT_EQUAL_STRING("abc123", out);

    TEST_ASSERT_EQUAL(ESP_OK, portal_api_cookie_value(
                                  "theme=dark; portal_session=def456; lang=es", out,
                                  sizeof(out)));
    TEST_ASSERT_EQUAL_STRING("def456", out);

    /* No space after the separator is equally valid. */
    TEST_ASSERT_EQUAL(ESP_OK,
                      portal_api_cookie_value("a=b;portal_session=xyz", out, sizeof(out)));
    TEST_ASSERT_EQUAL_STRING("xyz", out);

    TEST_ASSERT_EQUAL(ESP_ERR_NOT_FOUND,
                      portal_api_cookie_value("theme=dark; lang=es", out, sizeof(out)));
    /* A name that merely ends like the session cookie is not the cookie. */
    TEST_ASSERT_EQUAL(ESP_ERR_NOT_FOUND,
                      portal_api_cookie_value("other_portal_session=x", out, sizeof(out)));
    TEST_ASSERT_EQUAL(ESP_ERR_NOT_FOUND, portal_api_cookie_value("", out, sizeof(out)));

    /* 50-character value into a 64-byte buffer: fits. */
    TEST_ASSERT_EQUAL(ESP_OK, portal_api_cookie_value(
                                  "portal_session=01234567890123456789012345678901234567890123456789",
                                  out, sizeof(out)));
    /* 64-character value plus terminator into a 64-byte buffer: too small. */
    TEST_ASSERT_EQUAL(ESP_ERR_INVALID_SIZE, portal_api_cookie_value(
                                                "portal_session=0123456789012345678901234567890123456789012345678901234567890123",
                                                out, sizeof(out)));
    TEST_ASSERT_EQUAL(ESP_ERR_INVALID_SIZE,
                      portal_api_cookie_value("portal_session=abc", out, 3u));
    TEST_ASSERT_EQUAL(ESP_OK,
                      portal_api_cookie_value("portal_session=abc", out, 4u));
    TEST_ASSERT_EQUAL_STRING("abc", out);

    TEST_ASSERT_EQUAL(ESP_ERR_INVALID_ARG,
                      portal_api_cookie_value(NULL, out, sizeof(out)));
    TEST_ASSERT_EQUAL(ESP_ERR_INVALID_ARG,
                      portal_api_cookie_value("portal_session=abc", NULL, sizeof(out)));
}

TEST_CASE("error json wraps the stable error key", "[portal_api]")
{
    char out[64];

    TEST_ASSERT_EQUAL(ESP_OK, portal_api_error_json("no-autorizado", out, sizeof(out)));
    TEST_ASSERT_EQUAL_STRING("{\"error\":\"no-autorizado\"}", out);

    TEST_ASSERT_EQUAL(ESP_ERR_INVALID_SIZE,
                      portal_api_error_json("no-autorizado", out, 10u));
    TEST_ASSERT_EQUAL(ESP_ERR_INVALID_ARG,
                      portal_api_error_json(NULL, out, sizeof(out)));
}
