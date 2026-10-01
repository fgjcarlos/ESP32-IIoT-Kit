#include <string.h>

#include "esp_err.h"
#include "nvs_flash.h"
#include "unity.h"

#include "sparkplug_b.h"
#include "sparkplug_b_payload.h"
#include "sparkplug_b_state.h"
#include "sparkplug_b_topics.h"

TEST_CASE("seq wraps at 255", "[sparkplug_b]")
{
    sparkplug_b_state_reset();
    for (unsigned i = 0; i < 255; ++i) {
        (void)sparkplug_b_seq_next();
    }
    TEST_ASSERT_EQUAL_UINT64(0, sparkplug_b_seq_next());
}

TEST_CASE("topic nbirth", "[sparkplug_b]")
{
    char topic[96];
    TEST_ASSERT_EQUAL(ESP_OK, sparkplug_b_topic_nbirth("iiot-kit", "GW-AABB", topic, sizeof(topic)));
    TEST_ASSERT_EQUAL_STRING("spBv1.0/iiot-kit/NBIRTH/GW-AABB", topic);
}

TEST_CASE("topic ndeath", "[sparkplug_b]")
{
    char topic[96];
    TEST_ASSERT_EQUAL(ESP_OK, sparkplug_b_topic_ndeath("iiot-kit", "GW-AABB", topic, sizeof(topic)));
    TEST_ASSERT_EQUAL_STRING("spBv1.0/iiot-kit/NDEATH/GW-AABB", topic);
}

TEST_CASE("topic ncmd", "[sparkplug_b]")
{
    char topic[96];
    TEST_ASSERT_EQUAL(ESP_OK, sparkplug_b_topic_ncmd("iiot-kit", "GW-AABB", topic, sizeof(topic)));
    TEST_ASSERT_EQUAL_STRING("spBv1.0/iiot-kit/NCMD/GW-AABB", topic);
}

TEST_CASE("topic dcmd", "[sparkplug_b]")
{
    char topic[128];
    TEST_ASSERT_EQUAL(ESP_OK, sparkplug_b_topic_dcmd("iiot-kit", "GW-AABB", "N-CCDD", topic, sizeof(topic)));
    TEST_ASSERT_EQUAL_STRING("spBv1.0/iiot-kit/DCMD/GW-AABB/N-CCDD", topic);
}

TEST_CASE("bd_seq round-trip", "[sparkplug_b]")
{
    nvs_handle_t handle;
    TEST_ASSERT_EQUAL(ESP_OK, nvs_flash_init());
    TEST_ASSERT_EQUAL(ESP_OK, nvs_open("mqtt", NVS_READWRITE, &handle));
    TEST_ASSERT_EQUAL(ESP_OK, sparkplug_b_bd_seq_save(handle, 42));
    uint64_t value = 0;
    TEST_ASSERT_EQUAL(ESP_OK, sparkplug_b_bd_seq_load(handle, &value));
    TEST_ASSERT_EQUAL_UINT64(42, value);
    TEST_ASSERT_EQUAL(ESP_OK, sparkplug_b_bd_seq_save(handle, 7));
    TEST_ASSERT_EQUAL(ESP_OK, sparkplug_b_bd_seq_load(handle, &value));
    TEST_ASSERT_EQUAL_UINT64(7, value);
    nvs_close(handle);
}

TEST_CASE("nbirth encode round-trip", "[sparkplug_b]")
{
    uint8_t buf[SPARKPLUG_B_PAYLOAD_MAX_BYTES] = {0};
    size_t encoded_len = 0;
    TEST_ASSERT_EQUAL(ESP_OK, sparkplug_b_encode_nbirth(buf, sizeof(buf), 7, 1234, &encoded_len));
    TEST_ASSERT_EQUAL(ESP_OK, sparkplug_b_decode_payload(buf, encoded_len));
    TEST_ASSERT_EQUAL_size_t(2, sparkplug_b_payload_metric_count());
    bool has_reserved_alias = false;
    bool has_uptime_name = false;
    const uint8_t alias_zero[] = {0x10, 0x00};
    for (size_t i = 0; i + sizeof(alias_zero) <= encoded_len; ++i) {
        if (memcmp(&buf[i], alias_zero, sizeof(alias_zero)) == 0) {
            has_reserved_alias = true;
        }
        if (i + sizeof("Inputs/UptimeMs") - 1 <= encoded_len &&
            memcmp(&buf[i], "Inputs/UptimeMs", sizeof("Inputs/UptimeMs") - 1) == 0) {
            has_uptime_name = true;
        }
    }
    TEST_ASSERT_TRUE(has_reserved_alias);
    TEST_ASSERT_TRUE(has_uptime_name);
}
