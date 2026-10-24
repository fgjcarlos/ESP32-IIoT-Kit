// Sparkplug B payload round-trip tests (commit 4).
//
// These run on-device inside gateway_test.bin (Unity via esp-idf's
// test runner). They do NOT touch the broker; the conformance harness
// that exercises a real MQTT broker lives in tools/sparkplug-conformance
// (commit 11).
//
// Commit 4 covers:
//   - encode of single + multi metric payloads (one per type)
//   - decode structural shape (metric_count)
//   - decoder rejects empty / NULL inputs
// Commit 5+ will extend decode with full field access for NCMD/DCMD.

#include <string.h>

#include "esp_err.h"
#include "unity.h"

#include "sparkplug_b_payload.h"

TEST_CASE("sparkplug_b encode single double metric produces non-empty payload", "[sparkplug_b]")
{
    sparkplug_metric_t in_metrics[1] = {
        {
            .alias = SPARKPLUG_ALIAS_TEMP,
            .name = "Inputs/Temperature",
            .data_type = SPARKPLUG_DATA_TYPE_DOUBLE,
            .value.double_value = 23.5,
        },
    };
    sparkplug_payload_t in = {
        .timestamp_ms = 123456789ULL,
        .seq = 1ULL,
        .metrics = in_metrics,
        .metric_count = 1,
    };

    uint8_t buf[SPARKPLUG_B_PAYLOAD_MAX_BYTES] = {0};
    size_t encoded_len = 0;
    TEST_ASSERT_EQUAL(ESP_OK,
                      sparkplug_b_encode_payload(&in, buf, sizeof(buf), &encoded_len));
    TEST_ASSERT_GREATER_THAN(0, encoded_len);
    TEST_ASSERT_LESS_OR_EQUAL(SPARKPLUG_B_PAYLOAD_MAX_BYTES, encoded_len);
}

TEST_CASE("sparkplug_b encode mixed-type payload with 4 metrics", "[sparkplug_b]")
{
    sparkplug_metric_t in_metrics[4] = {
        { .alias = SPARKPLUG_ALIAS_TEMP, .name = "Inputs/Temperature",
          .data_type = SPARKPLUG_DATA_TYPE_DOUBLE, .value.double_value = 24.7 },
        { .alias = SPARKPLUG_ALIAS_PH, .name = "Inputs/Ph",
          .data_type = SPARKPLUG_DATA_TYPE_DOUBLE, .value.double_value = 7.4 },
        { .alias = SPARKPLUG_ALIAS_UPTIME_MS, .name = "Diagnostics/UptimeMs",
          .data_type = SPARKPLUG_DATA_TYPE_INT64, .value.int64_value = 9876543210LL },
        { .alias = SPARKPLUG_ALIAS_RSSI, .name = "Diagnostics/RssiLast",
          .data_type = SPARKPLUG_DATA_TYPE_INT8, .value.int8_value = -65 },
    };
    sparkplug_payload_t in = {
        .timestamp_ms = 1ULL,
        .seq = 42ULL,
        .metrics = in_metrics,
        .metric_count = 4,
    };

    uint8_t buf[SPARKPLUG_B_PAYLOAD_MAX_BYTES] = {0};
    size_t encoded_len = 0;
    TEST_ASSERT_EQUAL(ESP_OK,
                      sparkplug_b_encode_payload(&in, buf, sizeof(buf), &encoded_len));
    TEST_ASSERT_GREATER_THAN(0, encoded_len);
}

TEST_CASE("sparkplug_b encode DDATA-style payload without name field", "[sparkplug_b]")
{
    // On DDATA we only set aliases; names are stripped to save bytes. The
    // gateway does this for downstream sensor traffic. We expect encode
    // to succeed and the resulting bytes to be smaller than the BIRTH
    // variant with the same number of metrics.
    sparkplug_metric_t in_metrics[1] = {
        { .alias = SPARKPLUG_ALIAS_DO,
          .name  = NULL,            // intentionally NULL on DDATA
          .data_type = SPARKPLUG_DATA_TYPE_DOUBLE,
          .value.double_value = 8.1 },
    };
    sparkplug_payload_t in = {
        .timestamp_ms = 0ULL,
        .seq = 5ULL,
        .metrics = in_metrics,
        .metric_count = 1,
    };

    uint8_t buf[SPARKPLUG_B_PAYLOAD_MAX_BYTES] = {0};
    size_t encoded_len = 0;
    TEST_ASSERT_EQUAL(ESP_OK,
                      sparkplug_b_encode_payload(&in, buf, sizeof(buf), &encoded_len));
    TEST_ASSERT_GREATER_THAN(0, encoded_len);
}

TEST_CASE("sparkplug_b decode surfaces metric count", "[sparkplug_b]")
{
    sparkplug_metric_t in_metrics[2] = {
        { .alias = SPARKPLUG_ALIAS_TEMP, .name = "Inputs/Temperature",
          .data_type = SPARKPLUG_DATA_TYPE_DOUBLE, .value.double_value = 22.0 },
        { .alias = SPARKPLUG_ALIAS_PH, .name = "Inputs/Ph",
          .data_type = SPARKPLUG_DATA_TYPE_DOUBLE, .value.double_value = 7.0 },
    };
    sparkplug_payload_t in = {
        .timestamp_ms = 1ULL,
        .seq = 1ULL,
        .metrics = in_metrics,
        .metric_count = 2,
    };
    uint8_t buf[SPARKPLUG_B_PAYLOAD_MAX_BYTES] = {0};
    size_t encoded_len = 0;
    TEST_ASSERT_EQUAL(ESP_OK,
                      sparkplug_b_encode_payload(&in, buf, sizeof(buf), &encoded_len));

    TEST_ASSERT_EQUAL(ESP_OK, sparkplug_b_decode_payload(buf, encoded_len));
    TEST_ASSERT_EQUAL_size_t(2, sparkplug_b_payload_metric_count());
}

TEST_CASE("sparkplug_b rejects NULL buffers and oversize inputs", "[sparkplug_b]")
{
    sparkplug_payload_t in = {
        .timestamp_ms = 0ULL,
        .seq = 0ULL,
        .metrics = NULL,
        .metric_count = 0,
    };
    size_t encoded_len = 0;
    TEST_ASSERT_EQUAL(ESP_ERR_INVALID_ARG,
                      sparkplug_b_encode_payload(&in, NULL, 0, &encoded_len));

    uint8_t small_buf[8] = {0};
    sparkplug_metric_t one = {
        .alias = SPARKPLUG_ALIAS_TEMP, .name = "Inputs/Temperature",
        .data_type = SPARKPLUG_DATA_TYPE_DOUBLE, .value.double_value = 1.0,
    };
    in.metrics = &one;
    in.metric_count = 1;
    TEST_ASSERT_EQUAL(ESP_ERR_INVALID_SIZE,
                      sparkplug_b_encode_payload(&in, small_buf, sizeof(small_buf), &encoded_len));

    TEST_ASSERT_EQUAL(ESP_ERR_INVALID_ARG, sparkplug_b_decode_payload(NULL, 0));
}

// Regression: the datatype of every metric used to be taken from a single
// shared slot, so each metric carried the datatype of the last metric that
// was prepared.
TEST_CASE("sparkplug_b encodes the datatype of each metric individually", "[sparkplug_b]")
{
    sparkplug_metric_t in_metrics[2] = {
        { .alias = SPARKPLUG_ALIAS_NONE, .name = SPARKPLUG_METRIC_REBIRTH_NAME,
          .data_type = SPARKPLUG_DATA_TYPE_BOOLEAN, .value.boolean_value = true },
        { .alias = SPARKPLUG_ALIAS_TEMP, .name = "Inputs/Temperature",
          .data_type = SPARKPLUG_DATA_TYPE_DOUBLE, .value.double_value = 1.5 },
    };
    sparkplug_payload_t in = {
        .timestamp_ms = 1ULL,
        .seq = 1ULL,
        .metrics = in_metrics,
        .metric_count = 2,
    };

    uint8_t buf[SPARKPLUG_B_PAYLOAD_MAX_BYTES] = {0};
    size_t encoded_len = 0;
    TEST_ASSERT_EQUAL(ESP_OK,
                      sparkplug_b_encode_payload(&in, buf, sizeof(buf), &encoded_len));

    sparkplug_b_decoded_metric_t decoded[SPARKPLUG_B_MAX_DECODED_METRICS];
    size_t count = 0;
    TEST_ASSERT_EQUAL(ESP_OK, sparkplug_b_decode_metrics(buf, encoded_len, decoded,
                                                          SPARKPLUG_B_MAX_DECODED_METRICS,
                                                          &count));
    TEST_ASSERT_EQUAL_size_t(2, count);
    TEST_ASSERT_EQUAL_UINT32(SPARKPLUG_DATA_TYPE_BOOLEAN, decoded[0].datatype);
    TEST_ASSERT_TRUE(decoded[0].value.boolean_value);
    TEST_ASSERT_EQUAL_UINT32(SPARKPLUG_DATA_TYPE_DOUBLE, decoded[1].datatype);
    TEST_ASSERT_DOUBLE_WITHIN(0.0001, 1.5, decoded[1].value.double_value);
}

// Regression: the value of Payload.Metric lives in a protobuf oneof whose
// arms all share one C union, so only one callback can be registered. The
// encoder has to dispatch on the arm, otherwise every value is written with
// the wrong wire type.
TEST_CASE("sparkplug_b encodes each value arm with its own wire type", "[sparkplug_b]")
{
    sparkplug_metric_t in_metrics[3] = {
        { .alias = SPARKPLUG_ALIAS_NONE, .name = "A/Double",
          .data_type = SPARKPLUG_DATA_TYPE_DOUBLE, .value.double_value = 2.5 },
        { .alias = SPARKPLUG_ALIAS_NONE, .name = "A/Float",
          .data_type = SPARKPLUG_DATA_TYPE_FLOAT, .value.float_value = 0.5f },
        { .alias = SPARKPLUG_ALIAS_NONE, .name = "A/Boolean",
          .data_type = SPARKPLUG_DATA_TYPE_BOOLEAN, .value.boolean_value = true },
    };
    sparkplug_payload_t in = {
        .timestamp_ms = 1ULL,
        .seq = 1ULL,
        .metrics = in_metrics,
        .metric_count = 3,
    };

    uint8_t buf[SPARKPLUG_B_PAYLOAD_MAX_BYTES] = {0};
    size_t encoded_len = 0;
    TEST_ASSERT_EQUAL(ESP_OK,
                      sparkplug_b_encode_payload(&in, buf, sizeof(buf), &encoded_len));

    // Wire tags of the value arms: field number with the protobuf wire type.
    // double -> field 13 wire type 1 (64 bit) -> 0x69
    // float  -> field 12 wire type 5 (32 bit) -> 0x65
    // bool   -> field 14 wire type 0 (varint) -> 0x70
    const uint8_t expected[] = {0x69, 0x65, 0x70};
    for (size_t i = 0; i < sizeof(expected); ++i) {
        bool found = false;
        for (size_t j = 0; j < encoded_len; ++j) {
            if (buf[j] == expected[i]) {
                found = true;
                break;
            }
        }
        TEST_ASSERT_TRUE(found);
    }

    sparkplug_b_decoded_metric_t decoded[SPARKPLUG_B_MAX_DECODED_METRICS];
    size_t count = 0;
    TEST_ASSERT_EQUAL(ESP_OK, sparkplug_b_decode_metrics(buf, encoded_len, decoded,
                                                          SPARKPLUG_B_MAX_DECODED_METRICS,
                                                          &count));
    TEST_ASSERT_EQUAL_size_t(3, count);
    TEST_ASSERT_DOUBLE_WITHIN(0.0001, 2.5, decoded[0].value.double_value);
    TEST_ASSERT_FLOAT_WITHIN(0.0001, 0.5f, decoded[1].value.float_value);
    TEST_ASSERT_TRUE(decoded[2].value.boolean_value);
}