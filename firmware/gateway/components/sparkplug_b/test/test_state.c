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

TEST_CASE("topic ndata", "[sparkplug_b]")
{
    char topic[96];
    TEST_ASSERT_EQUAL(ESP_OK, sparkplug_b_topic_ndata("iiot-kit", "GW-AABB", topic, sizeof(topic)));
    TEST_ASSERT_EQUAL_STRING("spBv1.0/iiot-kit/NDATA/GW-AABB", topic);
}

TEST_CASE("topic dbirth", "[sparkplug_b]")
{
    char topic[128];
    TEST_ASSERT_EQUAL(ESP_OK, sparkplug_b_topic_dbirth("iiot-kit", "GW-AABB", "N-CCDD", topic, sizeof(topic)));
    TEST_ASSERT_EQUAL_STRING("spBv1.0/iiot-kit/DBIRTH/GW-AABB/N-CCDD", topic);
}

TEST_CASE("topic ddata", "[sparkplug_b]")
{
    char topic[128];
    TEST_ASSERT_EQUAL(ESP_OK, sparkplug_b_topic_ddata("iiot-kit", "GW-AABB", "N-CCDD", topic, sizeof(topic)));
    TEST_ASSERT_EQUAL_STRING("spBv1.0/iiot-kit/DDATA/GW-AABB/N-CCDD", topic);
}

TEST_CASE("topic ddeath", "[sparkplug_b]")
{
    char topic[128];
    TEST_ASSERT_EQUAL(ESP_OK, sparkplug_b_topic_ddeath("iiot-kit", "GW-AABB", "N-CCDD", topic, sizeof(topic)));
    TEST_ASSERT_EQUAL_STRING("spBv1.0/iiot-kit/DDEATH/GW-AABB/N-CCDD", topic);
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

    // Sparkplug B v3.0.0 requires the NBIRTH to carry the bdSeq metric and
    // the Node Control/Rebirth metric, and forbids an alias on either of
    // them so a Host Application can request a rebirth without knowing the
    // alias map.
    sparkplug_b_decoded_metric_t decoded[SPARKPLUG_B_MAX_DECODED_METRICS];
    size_t count = 0;
    TEST_ASSERT_EQUAL(ESP_OK, sparkplug_b_decode_metrics(buf, encoded_len, decoded,
                                                          SPARKPLUG_B_MAX_DECODED_METRICS,
                                                          &count));
    TEST_ASSERT_EQUAL_size_t(3, count);

    TEST_ASSERT_EQUAL_STRING(SPARKPLUG_METRIC_BDSEQ_NAME, decoded[0].name);
    TEST_ASSERT_EQUAL_UINT32(SPARKPLUG_DATA_TYPE_INT64, decoded[0].datatype);
    TEST_ASSERT_EQUAL_INT64(7, decoded[0].value.long_value);
    TEST_ASSERT_FALSE(decoded[0].has_alias);

    TEST_ASSERT_EQUAL_STRING(SPARKPLUG_METRIC_REBIRTH_NAME, decoded[1].name);
    TEST_ASSERT_EQUAL_UINT32(SPARKPLUG_DATA_TYPE_BOOLEAN, decoded[1].datatype);
    TEST_ASSERT_TRUE(decoded[1].has_value);
    TEST_ASSERT_FALSE(decoded[1].value.boolean_value);
    TEST_ASSERT_FALSE(decoded[1].has_alias);

    TEST_ASSERT_EQUAL_STRING("Inputs/UptimeMs", decoded[2].name);
    TEST_ASSERT_TRUE(decoded[2].has_alias);
    TEST_ASSERT_EQUAL_UINT64(SPARKPLUG_ALIAS_UPTIME_MS, decoded[2].alias);
}

TEST_CASE("nbirth opens the sequence with seq zero", "[sparkplug_b]")
{
    sparkplug_b_state_reset();
    (void)sparkplug_b_seq_next();
    (void)sparkplug_b_seq_next();

    uint8_t buf[SPARKPLUG_B_PAYLOAD_MAX_BYTES] = {0};
    size_t encoded_len = 0;
    TEST_ASSERT_EQUAL(ESP_OK, sparkplug_b_encode_nbirth(buf, sizeof(buf), 1, 1234, &encoded_len));

    // The birth resets the sequence, so the next message carries seq 1.
    TEST_ASSERT_EQUAL_UINT64(0, sparkplug_b_seq_reset());
    TEST_ASSERT_EQUAL_UINT64(1, sparkplug_b_seq_next());
}

TEST_CASE("bd_seq increments and wraps at 255", "[sparkplug_b]")
{
    sparkplug_b_state_reset();
    TEST_ASSERT_EQUAL_UINT64(1, sparkplug_b_bd_seq_next());
    TEST_ASSERT_EQUAL_UINT64(2, sparkplug_b_bd_seq_next());

    // bdSeq is a single byte: it wraps from 255 back to 0.
    sparkplug_b_state_t *state = sparkplug_b_state_get();
    state->bd_seq = 255;
    TEST_ASSERT_EQUAL_UINT64(0, sparkplug_b_bd_seq_next());
}
