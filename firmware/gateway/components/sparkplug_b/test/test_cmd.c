// Command handling, device registry and payload decoder tests (issue #11,
// commits 7 and 8).
//
// These run on-device inside gateway_test.bin (Unity via esp-idf's test
// runner). The broker-facing behaviour is covered by the conformance harness
// in tools/sparkplug-conformance.

#include <stdio.h>
#include <string.h>

#include "esp_err.h"
#include "unity.h"

#include "protocol.h"
#include "sparkplug_b_cmd.h"
#include "sparkplug_b_device.h"
#include "sparkplug_b_payload.h"
#include "sparkplug_b_state.h"

// Encode a command payload and decode it back, the same way the MQTT
// receive path does it.
static size_t build_command_payload(uint8_t *buf, size_t buf_size,
                                    const sparkplug_metric_t *metric)
{
    sparkplug_payload_t payload = {
        .timestamp_ms = 1700000000000ULL,
        .seq = 0ULL,
        .metrics = metric,
        .metric_count = 1,
    };
    size_t encoded_len = 0;
    TEST_ASSERT_EQUAL(ESP_OK,
                      sparkplug_b_encode_payload(&payload, buf, buf_size, &encoded_len));
    return encoded_len;
}

TEST_CASE("decoder surfaces metric name, datatype and value", "[sparkplug_b]")
{
    sparkplug_metric_t metric = {
        .alias = SPARKPLUG_ALIAS_NONE,
        .name = SPARKPLUG_METRIC_REBIRTH_NAME,
        .data_type = SPARKPLUG_DATA_TYPE_BOOLEAN,
        .value.boolean_value = true,
    };
    uint8_t buf[SPARKPLUG_B_PAYLOAD_MAX_BYTES] = {0};
    size_t len = build_command_payload(buf, sizeof(buf), &metric);

    sparkplug_b_decoded_metric_t decoded[SPARKPLUG_B_MAX_DECODED_METRICS];
    size_t count = 0;
    TEST_ASSERT_EQUAL(ESP_OK, sparkplug_b_decode_metrics(buf, len, decoded,
                                                          SPARKPLUG_B_MAX_DECODED_METRICS,
                                                          &count));
    TEST_ASSERT_EQUAL_size_t(1, count);
    TEST_ASSERT_EQUAL_STRING(SPARKPLUG_METRIC_REBIRTH_NAME, decoded[0].name);
    TEST_ASSERT_EQUAL_UINT32(SPARKPLUG_DATA_TYPE_BOOLEAN, decoded[0].datatype);
    TEST_ASSERT_TRUE(decoded[0].has_value);
    TEST_ASSERT_TRUE(decoded[0].value.boolean_value);
}

TEST_CASE("decoder surfaces a double value", "[sparkplug_b]")
{
    sparkplug_metric_t metric = {
        .alias = SPARKPLUG_ALIAS_NONE,
        .name = "Inputs/Temperature",
        .data_type = SPARKPLUG_DATA_TYPE_DOUBLE,
        .value.double_value = 21.5,
    };
    uint8_t buf[SPARKPLUG_B_PAYLOAD_MAX_BYTES] = {0};
    size_t len = build_command_payload(buf, sizeof(buf), &metric);

    sparkplug_b_decoded_metric_t decoded[SPARKPLUG_B_MAX_DECODED_METRICS];
    size_t count = 0;
    TEST_ASSERT_EQUAL(ESP_OK, sparkplug_b_decode_metrics(buf, len, decoded,
                                                          SPARKPLUG_B_MAX_DECODED_METRICS,
                                                          &count));
    TEST_ASSERT_EQUAL_size_t(1, count);
    TEST_ASSERT_EQUAL_STRING("Inputs/Temperature", decoded[0].name);
    TEST_ASSERT_EQUAL_UINT32(SPARKPLUG_DATA_TYPE_DOUBLE, decoded[0].datatype);
    TEST_ASSERT_TRUE(decoded[0].has_value);
    TEST_ASSERT_DOUBLE_WITHIN(0.001, 21.5, decoded[0].value.double_value);
}

TEST_CASE("decoder rejects malformed input", "[sparkplug_b]")
{
    sparkplug_b_decoded_metric_t decoded[SPARKPLUG_B_MAX_DECODED_METRICS];
    size_t count = 0;
    TEST_ASSERT_EQUAL(ESP_ERR_INVALID_ARG,
                      sparkplug_b_decode_metrics(NULL, 0, decoded,
                                                SPARKPLUG_B_MAX_DECODED_METRICS, &count));

    uint8_t garbage[8] = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF};
    TEST_ASSERT_EQUAL(ESP_ERR_INVALID_SIZE,
                      sparkplug_b_decode_metrics(garbage, sizeof(garbage), decoded,
                                                SPARKPLUG_B_MAX_DECODED_METRICS, &count));
}

TEST_CASE("ncmd rebirth request is recognised", "[sparkplug_b]")
{
    sparkplug_b_decoded_metric_t metrics[2] = {0};
    snprintf(metrics[0].name, sizeof(metrics[0].name), "%s", SPARKPLUG_METRIC_REBIRTH_NAME);
    metrics[0].datatype = SPARKPLUG_DATA_TYPE_BOOLEAN;
    metrics[0].has_value = true;
    metrics[0].value.boolean_value = true;

    sparkplug_b_command_t cmd;
    TEST_ASSERT_EQUAL(ESP_OK, sparkplug_b_cmd_parse(metrics, 1, false, &cmd));
    TEST_ASSERT_EQUAL(SPARKPLUG_CMD_REBIRTH, cmd.kind);
}

TEST_CASE("ncmd reboot request is recognised", "[sparkplug_b]")
{
    sparkplug_b_decoded_metric_t metrics[1] = {0};
    snprintf(metrics[0].name, sizeof(metrics[0].name), "%s", SPARKPLUG_METRIC_REBOOT_NAME);
    metrics[0].datatype = SPARKPLUG_DATA_TYPE_BOOLEAN;
    metrics[0].has_value = true;
    metrics[0].value.boolean_value = true;

    sparkplug_b_command_t cmd;
    TEST_ASSERT_EQUAL(ESP_OK, sparkplug_b_cmd_parse(metrics, 1, false, &cmd));
    TEST_ASSERT_EQUAL(SPARKPLUG_CMD_NODE_REBOOT, cmd.kind);
}

TEST_CASE("ncmd with a false rebirth flag is not a command", "[sparkplug_b]")
{
    // Every NBIRTH carries Node Control/Rebirth set to false, so a false
    // value must never be mistaken for a rebirth request.
    sparkplug_b_decoded_metric_t metrics[1] = {0};
    snprintf(metrics[0].name, sizeof(metrics[0].name), "%s", SPARKPLUG_METRIC_REBIRTH_NAME);
    metrics[0].datatype = SPARKPLUG_DATA_TYPE_BOOLEAN;
    metrics[0].has_value = true;
    metrics[0].value.boolean_value = false;

    sparkplug_b_command_t cmd;
    TEST_ASSERT_EQUAL(ESP_OK, sparkplug_b_cmd_parse(metrics, 1, false, &cmd));
    TEST_ASSERT_EQUAL(SPARKPLUG_CMD_UNSUPPORTED, cmd.kind);
}

TEST_CASE("dcmd device reboot becomes an ESP-NOW config frame", "[sparkplug_b]")
{
    sparkplug_b_decoded_metric_t metrics[1] = {0};
    snprintf(metrics[0].name, sizeof(metrics[0].name), "%s",
             SPARKPLUG_METRIC_DEVICE_REBOOT_NAME);
    metrics[0].datatype = SPARKPLUG_DATA_TYPE_BOOLEAN;
    metrics[0].has_value = true;
    metrics[0].value.boolean_value = true;

    sparkplug_b_command_t cmd;
    TEST_ASSERT_EQUAL(ESP_OK, sparkplug_b_cmd_parse(metrics, 1, true, &cmd));
    TEST_ASSERT_EQUAL(SPARKPLUG_CMD_DEVICE_REBOOT, cmd.kind);

    uint8_t mac[6] = {0xAA, 0xBB, 0xCC, 0xDD, 0xEE, 0xFF};
    uint8_t frame[SPARKPLUG_B_CMD_FRAME_MAX];
    size_t frame_len = 0;
    TEST_ASSERT_EQUAL(ESP_OK, sparkplug_b_cmd_to_espnow(&cmd, mac, 7,
                                                       frame, sizeof(frame), &frame_len));
    TEST_ASSERT_EQUAL_size_t(sizeof(msg_config_t), frame_len);

    msg_config_t decoded;
    TEST_ASSERT_EQUAL(ESP_OK, protocol_deserialize(frame, frame_len, MSG_TYPE_CONFIG, &decoded));
    TEST_ASSERT_EQUAL(MSG_TYPE_CONFIG, decoded.header.msg_type);
    TEST_ASSERT_EQUAL(MSG_CONFIG_FLAG_REBOOT, decoded.flags);
    TEST_ASSERT_EQUAL_UINT16(7, decoded.header.sequence);
    TEST_ASSERT_EQUAL_UINT8(0xAA, decoded.header.node_mac[0]);
    TEST_ASSERT_EQUAL_UINT8(0xFF, decoded.header.node_mac[5]);
}

TEST_CASE("dcmd numeric write becomes a sleep interval change", "[sparkplug_b]")
{
    sparkplug_b_decoded_metric_t metrics[1] = {0};
    snprintf(metrics[0].name, sizeof(metrics[0].name), "%s", "Inputs/SleepIntervalSec");
    metrics[0].datatype = SPARKPLUG_DATA_TYPE_INT64;
    metrics[0].has_value = true;
    metrics[0].value.long_value = 300;

    sparkplug_b_command_t cmd;
    TEST_ASSERT_EQUAL(ESP_OK, sparkplug_b_cmd_parse(metrics, 1, true, &cmd));
    TEST_ASSERT_EQUAL(SPARKPLUG_CMD_SET_SLEEP, cmd.kind);
    TEST_ASSERT_EQUAL_UINT16(300, cmd.sleep_interval_s);

    uint8_t mac[6] = {1, 2, 3, 4, 5, 6};
    uint8_t frame[SPARKPLUG_B_CMD_FRAME_MAX];
    size_t frame_len = 0;
    TEST_ASSERT_EQUAL(ESP_OK, sparkplug_b_cmd_to_espnow(&cmd, mac, 1,
                                                       frame, sizeof(frame), &frame_len));
    msg_config_t decoded;
    TEST_ASSERT_EQUAL(ESP_OK, protocol_deserialize(frame, frame_len, MSG_TYPE_CONFIG, &decoded));
    TEST_ASSERT_EQUAL(MSG_CONFIG_FLAG_SLEEP_INTERVAL, decoded.flags);
    TEST_ASSERT_EQUAL_UINT16(300, decoded.new_sleep_interval);
}

TEST_CASE("dcmd with an unknown metric is unsupported", "[sparkplug_b]")
{
    sparkplug_b_decoded_metric_t metrics[1] = {0};
    snprintf(metrics[0].name, sizeof(metrics[0].name), "%s", "Some/UnknownMetric");
    metrics[0].datatype = SPARKPLUG_DATA_TYPE_BOOLEAN;
    metrics[0].has_value = true;
    metrics[0].value.boolean_value = true;

    sparkplug_b_command_t cmd;
    TEST_ASSERT_EQUAL(ESP_OK, sparkplug_b_cmd_parse(metrics, 1, true, &cmd));
    TEST_ASSERT_EQUAL(SPARKPLUG_CMD_UNSUPPORTED, cmd.kind);
}

TEST_CASE("node scoped commands have no ESP-NOW equivalent", "[sparkplug_b]")
{
    sparkplug_b_command_t cmd = { .kind = SPARKPLUG_CMD_REBIRTH };
    uint8_t mac[6] = {0};
    uint8_t frame[SPARKPLUG_B_CMD_FRAME_MAX];
    size_t frame_len = 0;
    TEST_ASSERT_EQUAL(ESP_ERR_NOT_SUPPORTED,
                      sparkplug_b_cmd_to_espnow(&cmd, mac, 0, frame, sizeof(frame), &frame_len));
}

TEST_CASE("device registry tracks devices and expires silent ones", "[sparkplug_b]")
{
    TEST_ASSERT_EQUAL(ESP_OK, sparkplug_b_devices_init());
    TEST_ASSERT_EQUAL_size_t(0, sparkplug_b_devices_count());

    uint8_t mac[6] = {0x10, 0x20, 0x30, 0x40, 0x50, 0x60};
    TEST_ASSERT_EQUAL(ESP_OK, sparkplug_b_devices_touch("N-5060", mac, 1000));
    TEST_ASSERT_EQUAL_size_t(1, sparkplug_b_devices_count());

    // Touching the same device again must not create a second entry.
    TEST_ASSERT_EQUAL(ESP_OK, sparkplug_b_devices_touch("N-5060", mac, 2000));
    TEST_ASSERT_EQUAL_size_t(1, sparkplug_b_devices_count());

    size_t index = 0;
    TEST_ASSERT_EQUAL(ESP_OK, sparkplug_b_devices_find("N-5060", &index));
    TEST_ASSERT_EQUAL(ESP_ERR_NOT_FOUND, sparkplug_b_devices_find("N-9999", &index));

    // Still inside the timeout window.
    size_t expired[SPARKPLUG_B_MAX_DEVICES];
    TEST_ASSERT_EQUAL_size_t(0, sparkplug_b_devices_expire(5000, 10, expired,
                                                            SPARKPLUG_B_MAX_DEVICES));

    // Past the timeout the device goes stale and is reported once.
    TEST_ASSERT_EQUAL_size_t(1, sparkplug_b_devices_expire(20000, 10, expired,
                                                            SPARKPLUG_B_MAX_DEVICES));
    TEST_ASSERT_EQUAL_size_t(0, sparkplug_b_devices_expire(30000, 10, expired,
                                                            SPARKPLUG_B_MAX_DEVICES));

    sparkplug_b_device_t device;
    TEST_ASSERT_EQUAL(ESP_OK, sparkplug_b_devices_get(index, &device));
    TEST_ASSERT_EQUAL(SPARKPLUG_DEVICE_STALE, device.state);
    TEST_ASSERT_EQUAL_UINT8(0x10, device.mac[0]);
    TEST_ASSERT_EQUAL_UINT8(0x60, device.mac[5]);
}

TEST_CASE("device registry stores the metric set used to replay a birth", "[sparkplug_b]")
{
    TEST_ASSERT_EQUAL(ESP_OK, sparkplug_b_devices_init());
    uint8_t mac[6] = {0x11, 0x22, 0x33, 0x44, 0x55, 0x66};
    TEST_ASSERT_EQUAL(ESP_OK, sparkplug_b_devices_touch("N-5566", mac, 1000));

    sparkplug_metric_t metrics[2] = {
        {
            .alias = SPARKPLUG_ALIAS_TEMP,
            .name = "Inputs/Temperature",
            .data_type = SPARKPLUG_DATA_TYPE_DOUBLE,
            .value.double_value = 21.5,
        },
        {
            .alias = SPARKPLUG_ALIAS_DEV_UPTIME_MS,
            .name = SPARKPLUG_METRIC_DEV_UPTIME_NAME,
            .data_type = SPARKPLUG_DATA_TYPE_INT64,
            .value.int64_value = 1234,
        },
    };
    TEST_ASSERT_EQUAL(ESP_OK, sparkplug_b_devices_store_birth("N-5566", metrics,
                                                               sizeof(metrics) / sizeof(metrics[0])));
    size_t index = 0;
    TEST_ASSERT_EQUAL(ESP_OK, sparkplug_b_devices_find("N-5566", &index));
    sparkplug_b_device_t device;
    TEST_ASSERT_EQUAL(ESP_OK, sparkplug_b_devices_get(index, &device));
    TEST_ASSERT_EQUAL_size_t(2, device.metric_count);
    TEST_ASSERT_EQUAL_UINT64(SPARKPLUG_ALIAS_TEMP, device.metrics[0].alias);
    TEST_ASSERT_EQUAL_STRING("Inputs/Temperature", device.metrics[0].name);
    TEST_ASSERT_EQUAL_UINT64(SPARKPLUG_ALIAS_DEV_UPTIME_MS, device.metrics[1].alias);

    // A metric set that does not fit is rejected instead of being truncated.
    sparkplug_metric_t oversized[SPARKPLUG_B_DEVICE_METRICS_MAX + 1] = {0};
    TEST_ASSERT_EQUAL(ESP_ERR_INVALID_SIZE,
                      sparkplug_b_devices_store_birth("N-5566", oversized,
                                                      sizeof(oversized) / sizeof(oversized[0])));
}

TEST_CASE("death certificate carries bdSeq and no sequence number", "[sparkplug_b]")
{
    uint8_t buf[SPARKPLUG_B_PAYLOAD_MAX_BYTES] = {0};
    size_t len = 0;
    TEST_ASSERT_EQUAL(ESP_OK, sparkplug_b_encode_death(buf, sizeof(buf), 9, 1700000000000ULL, &len));

    sparkplug_b_decoded_metric_t decoded[SPARKPLUG_B_MAX_DECODED_METRICS];
    size_t count = 0;
    TEST_ASSERT_EQUAL(ESP_OK, sparkplug_b_decode_metrics(buf, len, decoded,
                                                          SPARKPLUG_B_MAX_DECODED_METRICS,
                                                          &count));
    TEST_ASSERT_EQUAL_size_t(1, count);
    TEST_ASSERT_EQUAL_STRING(SPARKPLUG_METRIC_BDSEQ_NAME, decoded[0].name);
    TEST_ASSERT_EQUAL_UINT32(SPARKPLUG_DATA_TYPE_INT64, decoded[0].datatype);
    TEST_ASSERT_EQUAL_INT64(9, decoded[0].value.long_value);
    // A death certificate must not carry an alias.
    TEST_ASSERT_FALSE(decoded[0].has_alias);
}
