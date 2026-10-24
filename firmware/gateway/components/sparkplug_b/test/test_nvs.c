#include <string.h>

#include "unity.h"

#include "sparkplug_b_nvs.h"

/*
 * NVS answers a key longer than `NVS_MAX_KEY_LEN` characters with
 * `ESP_ERR_NVS_KEY_TOO_LONG`, and the error is fatal for the configuration
 * load: the gateway used to fail with it right after boot because three keys
 * of the `mqtt` namespace were too long, and no MQTT session was ever
 * established.
 *
 * The host test runner does not link `nvs_flash`, so the limit cannot be
 * discovered there. This test walks the list of keys of the namespace and
 * fails as soon as one of them does not fit.
 */
TEST_CASE("every NVS key of the MQTT namespace fits the NVS key length", "[sparkplug_b]")
{
#define CHECK_KEY_LENGTH(key) \
    TEST_ASSERT_LESS_OR_EQUAL_UINT32((uint32_t)SPARKPLUG_B_NVS_KEY_MAX_LEN, \
                                      (uint32_t)strlen(key));
    SPARKPLUG_B_NVS_KEYS(CHECK_KEY_LENGTH)
#undef CHECK_KEY_LENGTH
}

/* The namespace name is subject to the same limit. */
TEST_CASE("the NVS namespace of the MQTT configuration fits the NVS key length", "[sparkplug_b]")
{
    TEST_ASSERT_LESS_OR_EQUAL_UINT32((uint32_t)SPARKPLUG_B_NVS_KEY_MAX_LEN,
                                      (uint32_t)strlen(SPARKPLUG_B_NVS_NAMESPACE));
    TEST_ASSERT_NOT_NULL(SPARKPLUG_B_NVS_NAMESPACE);
}
