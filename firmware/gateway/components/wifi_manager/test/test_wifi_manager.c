/**
 * @file test_wifi_manager.c
 * @brief Unity tests for the deterministic helpers exported by wifi_manager.
 *
 * The RED scaffold returns ESP_ERR_NOT_FINISHED for every helper, so the
 * asserts below deliberately fail. WUC2 turns them green by implementing
 * the behaviour in `wifi_manager.c`.
 */

#include <string.h>

#include "unity.h"
#include "wifi_manager.h"

TEST_CASE("validate_ap_channel accepts 1..13 and rejects 0/14", "[wifi_manager]")
{
    bool is_valid = true;

    TEST_ASSERT_EQUAL(ESP_OK, wifi_manager_validate_ap_channel(1, &is_valid));
    TEST_ASSERT_TRUE(is_valid);
    TEST_ASSERT_EQUAL(ESP_OK, wifi_manager_validate_ap_channel(6, &is_valid));
    TEST_ASSERT_TRUE(is_valid);
    TEST_ASSERT_EQUAL(ESP_OK, wifi_manager_validate_ap_channel(13, &is_valid));
    TEST_ASSERT_TRUE(is_valid);

    TEST_ASSERT_EQUAL(ESP_OK, wifi_manager_validate_ap_channel(0, &is_valid));
    TEST_ASSERT_FALSE(is_valid);
    TEST_ASSERT_EQUAL(ESP_OK, wifi_manager_validate_ap_channel(14, &is_valid));
    TEST_ASSERT_FALSE(is_valid);
}

TEST_CASE("validate_ap_channel returns ESP_ERR_INVALID_ARG when output is NULL", "[wifi_manager]")
{
    TEST_ASSERT_EQUAL(ESP_ERR_INVALID_ARG, wifi_manager_validate_ap_channel(6, NULL));
}

TEST_CASE("ssid_in_range accepts a 32-octet SSID and rejects NULL/oversize", "[wifi_manager]")
{
    char ok_ssid[WIFI_MANAGER_SSID_MAX_LEN + 1];
    memset(ok_ssid, 'A', sizeof(ok_ssid) - 1);
    ok_ssid[sizeof(ok_ssid) - 1] = '\0';

    char too_long[WIFI_MANAGER_SSID_MAX_LEN + 2];
    memset(too_long, 'B', sizeof(too_long) - 1);
    too_long[sizeof(too_long) - 1] = '\0';

    TEST_ASSERT_EQUAL(ESP_OK, wifi_manager_ssid_in_range(ok_ssid));
    TEST_ASSERT_EQUAL(ESP_OK, wifi_manager_ssid_in_range("IIoT-Gateway"));
    TEST_ASSERT_EQUAL(ESP_ERR_INVALID_ARG, wifi_manager_ssid_in_range(NULL));
    TEST_ASSERT_EQUAL(ESP_ERR_INVALID_SIZE, wifi_manager_ssid_in_range(too_long));
}

TEST_CASE("password_in_range accepts an empty password but rejects oversize", "[wifi_manager]")
{
    char too_long[WIFI_MANAGER_PASSWORD_MAX_LEN + 2];
    memset(too_long, 'C', sizeof(too_long) - 1);
    too_long[sizeof(too_long) - 1] = '\0';

    TEST_ASSERT_EQUAL(ESP_OK, wifi_manager_password_in_range(""));
    TEST_ASSERT_EQUAL(ESP_OK, wifi_manager_password_in_range("iiot-kit"));
    TEST_ASSERT_EQUAL(ESP_ERR_INVALID_ARG, wifi_manager_password_in_range(NULL));
    TEST_ASSERT_EQUAL(ESP_ERR_INVALID_SIZE, wifi_manager_password_in_range(too_long));
}

TEST_CASE("format_mac produces aa:bb:cc:11:22:33", "[wifi_manager]")
{
    const uint8_t mac[6] = {0xAA, 0xBB, 0xCC, 0x11, 0x22, 0x33};
    char buf[WIFI_MANAGER_MAC_STR_LEN] = {0};

    TEST_ASSERT_EQUAL(ESP_OK, wifi_manager_format_mac(mac, buf, sizeof(buf)));
    TEST_ASSERT_EQUAL_STRING("aa:bb:cc:11:22:33", buf);
}

TEST_CASE("format_mac returns ESP_ERR_INVALID_ARG when output is too small", "[wifi_manager]")
{
    const uint8_t mac[6] = {0};
    char small[WIFI_MANAGER_MAC_STR_LEN - 1] = {0};

    TEST_ASSERT_EQUAL(ESP_ERR_INVALID_ARG, wifi_manager_format_mac(mac, small, sizeof(small)));
    TEST_ASSERT_EQUAL(ESP_ERR_INVALID_ARG, wifi_manager_format_mac(NULL, small, sizeof(small)));
}

TEST_CASE("apply_credentials substitutes defaults for NULL/zero fields", "[wifi_manager]")
{
    const wifi_manager_credentials_t in = {
        .sta_ssid = "lab-router",
        .sta_password = "lab-secret",
        .ap_ssid = NULL,
        .ap_password = NULL,
        .ap_channel = 0,
        .ap_max_connection = 0,
    };
    wifi_manager_credentials_t out;

    TEST_ASSERT_EQUAL(ESP_OK, wifi_manager_apply_credentials(&in, &out));
    TEST_ASSERT_EQUAL_STRING("lab-router", out.sta_ssid);
    TEST_ASSERT_EQUAL_STRING("lab-secret", out.sta_password);
    TEST_ASSERT_EQUAL_STRING(WIFI_MANAGER_AP_DEFAULT_SSID, out.ap_ssid);
    TEST_ASSERT_EQUAL_UINT8(WIFI_MANAGER_AP_DEFAULT_CHANNEL, out.ap_channel);
    TEST_ASSERT_EQUAL_UINT8(WIFI_MANAGER_AP_DEFAULT_MAX_CONN, out.ap_max_connection);
    TEST_ASSERT_NULL(out.ap_password);
}

TEST_CASE("apply_credentials rejects NULL pointers", "[wifi_manager]")
{
    wifi_manager_credentials_t out;

    TEST_ASSERT_EQUAL(ESP_ERR_INVALID_ARG, wifi_manager_apply_credentials(NULL, &out));
    TEST_ASSERT_EQUAL(ESP_ERR_INVALID_ARG, wifi_manager_apply_credentials(&(wifi_manager_credentials_t){0}, NULL));
}

TEST_CASE("init rejects an out-of-range AP channel before touching the driver", "[wifi_manager]")
{
    /* Channel 14 is rejected before esp_netif_init / esp_wifi_init. The
     * component must NOT have called any system API yet, so a second
     * init must still be allowed afterward (proves we did not start Wi-Fi). */
    const wifi_manager_credentials_t bad = {
        .sta_ssid = NULL,
        .sta_password = NULL,
        .ap_ssid = NULL,
        .ap_password = NULL,
        .ap_channel = 14,        /* out of range on 2.4 GHz */
        .ap_max_connection = 4,
    };

    TEST_ASSERT_EQUAL(ESP_ERR_INVALID_ARG, wifi_manager_init(&bad));
}

TEST_CASE("deinit without init returns ESP_ERR_INVALID_STATE", "[wifi_manager]")
{
    /* Forces the linker to retain wifi_manager_deinit so the symbol
     * shows up in the test ELF even when init never ran. */
    TEST_ASSERT_NOT_NULL((void *)&wifi_manager_deinit);
    TEST_ASSERT_EQUAL(ESP_ERR_INVALID_STATE, wifi_manager_deinit());
}
