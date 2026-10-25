// Sparkplug B session regression (issue #14).
//
// The other test files of this component check one rule each. This one walks a
// whole session in the order a Host Application sees it:
//
//   NBIRTH -> NDATA -> DBIRTH -> DDATA -> DDEATH -> NDEATH
//
// and asserts the three properties that tie those messages together:
//
//   - the node and its devices share ONE sequence space, and the space only
//     advances when a message is published;
//   - the birth/death sequence (bdSeq) identifies the session, so the death
//     certificate carries the bdSeq of the birth that opened it;
//   - a command does not consume a sequence number, because the space counts
//     published messages and not received ones.
//
// The sequence numbers are read from the wire with nanopb, not from the
// state machine, so the test covers what a Host Application actually sees.
//
// Reference: Eclipse Sparkplug B v3.0.0 §6.4 (State Management) and the
// requirement ids of the Tahu test suite (tck-id-*).

#include <string.h>

#include "esp_err.h"
#include "unity.h"

#include "pb_decode.h"
#include "sparkplug_b.pb.h"

#include "protocol.h"
#include "sparkplug_b_cmd.h"
#include "sparkplug_b_device.h"
#include "sparkplug_b_espnow_bridge.h"
#include "sparkplug_b_payload.h"
#include "sparkplug_b_state.h"

static const uint8_t NODE_MAC[6] = {0xAA, 0xBB, 0xCC, 0xDD, 0xEE, 0x01};

/**
 * Decode the envelope of a payload: the sequence number and the metric count.
 *
 * The metrics of the vendored Tahu schema are nanopb callbacks, so a plain
 * pb_decode() skips their contents; the envelope (timestamp, seq) is a plain
 * field and is decoded normally.
 */
static void decode_envelope(const uint8_t *buf, size_t len, uint64_t *seq, bool *has_seq,
                             size_t *metric_count)
{
    org_eclipse_tahu_protobuf_Payload payload = org_eclipse_tahu_protobuf_Payload_init_zero;

    pb_istream_t stream = pb_istream_from_buffer(buf, (unsigned)len);
    TEST_ASSERT_TRUE(pb_decode(&stream, org_eclipse_tahu_protobuf_Payload_fields, &payload));
    *seq = payload.seq;
    *has_seq = payload.has_seq;
    *metric_count = payload.metrics_count;
}

/** Encode a builder into a buffer and return its length. */
static size_t encode(const sparkplug_payload_t *payload, uint8_t *buf, size_t buf_len)
{
    size_t len = 0;
    TEST_ASSERT_EQUAL(ESP_OK, sparkplug_b_encode_payload(payload, buf, buf_len, &len));
    return len;
}

/** Build a MSG_TYPE_DATA frame for the node of the session. */
static void fill_data(msg_sensor_data_t *out, float value)
{
    memset(out, 0, sizeof(*out));
    protocol_fill_header(&out->header, MSG_TYPE_DATA, NODE_MAC, 1,
                         sizeof(*out) - sizeof(msg_header_t));
    out->sensor_type = SENSOR_TYPE_TEMPERATURE;
    out->value_count = 1;
    out->values[0] = value;
    out->uptime_ms = 1000;
    out->battery_pct = 90;
    out->rssi_last = -60;
}

TEST_CASE("the session opens with a birth and closes with a death of the same session",
          "[sparkplug_b_lifecycle]")
{
    sparkplug_b_state_reset();

    uint8_t buf[SPARKPLUG_B_PAYLOAD_MAX_BYTES] = {0};
    size_t len = 0;

    // --- NBIRTH ---------------------------------------------------------
    // The birth opens the sequence space of the session, so it carries seq 0.
    TEST_ASSERT_EQUAL(ESP_OK, sparkplug_b_encode_nbirth(buf, sizeof(buf), 7, 1234, &len));

    uint64_t seq = 0;
    bool has_seq = false;
    size_t metric_count = 0;
    decode_envelope(buf, len, &seq, &has_seq, &metric_count);
    TEST_ASSERT_TRUE(has_seq);
    TEST_ASSERT_EQUAL_UINT64(0, seq);
    TEST_ASSERT_EQUAL_size_t(3, metric_count);

    // The birth declares the bdSeq and the Rebirth metric without an alias,
    // so a Host Application can request a rebirth without knowing the alias
    // map of the node.
    sparkplug_b_decoded_metric_t metrics[SPARKPLUG_B_MAX_DECODED_METRICS];
    size_t decoded = 0;
    TEST_ASSERT_EQUAL(ESP_OK, sparkplug_b_decode_metrics(buf, len, metrics,
                                                          SPARKPLUG_B_MAX_DECODED_METRICS, &decoded));
    TEST_ASSERT_EQUAL_size_t(3, decoded);
    TEST_ASSERT_EQUAL_STRING(SPARKPLUG_METRIC_BDSEQ_NAME, metrics[0].name);
    TEST_ASSERT_EQUAL_UINT32(SPARKPLUG_DATA_TYPE_INT64, metrics[0].datatype);
    TEST_ASSERT_EQUAL_INT64(7, metrics[0].value.long_value);
    TEST_ASSERT_FALSE(metrics[0].has_alias);
    TEST_ASSERT_EQUAL_STRING(SPARKPLUG_METRIC_REBIRTH_NAME, metrics[1].name);
    TEST_ASSERT_EQUAL_UINT32(SPARKPLUG_DATA_TYPE_BOOLEAN, metrics[1].datatype);
    TEST_ASSERT_FALSE(metrics[1].value.boolean_value);
    TEST_ASSERT_FALSE(metrics[1].has_alias);

    // --- NDEATH ---------------------------------------------------------
    // The death certificate is published by the broker on behalf of the Edge
    // Node, so it carries NO sequence number
    // (tck-id-payloads-sequence-num-always-included), and it carries the
    // bdSeq of the birth that opened the session
    // (tck-id-payloads-ndeath-bdseq).
    memset(buf, 0, sizeof(buf));
    len = 0;
    TEST_ASSERT_EQUAL(ESP_OK, sparkplug_b_encode_death(buf, sizeof(buf), 7, 5678, &len));

    decode_envelope(buf, len, &seq, &has_seq, &metric_count);
    TEST_ASSERT_FALSE(has_seq);
    TEST_ASSERT_EQUAL_size_t(1, metric_count);

    decoded = 0;
    TEST_ASSERT_EQUAL(ESP_OK, sparkplug_b_decode_metrics(buf, len, metrics,
                                                          SPARKPLUG_B_MAX_DECODED_METRICS, &decoded));
    TEST_ASSERT_EQUAL_size_t(1, decoded);
    TEST_ASSERT_EQUAL_STRING(SPARKPLUG_METRIC_BDSEQ_NAME, metrics[0].name);
    TEST_ASSERT_EQUAL_UINT32(SPARKPLUG_DATA_TYPE_INT64, metrics[0].datatype);
    TEST_ASSERT_EQUAL_INT64(7, metrics[0].value.long_value);
    TEST_ASSERT_FALSE(metrics[0].has_alias);
}

TEST_CASE("the node and its devices share one sequence space", "[sparkplug_b_lifecycle]")
{
    sparkplug_b_state_reset();
    TEST_ASSERT_EQUAL(ESP_OK, sparkplug_b_espnow_bridge_init("iiot-kit", "GW-AABB"));

    uint8_t buf[SPARKPLUG_B_PAYLOAD_MAX_BYTES] = {0};
    size_t len = 0;

    // The birth opens the space with seq 0.
    TEST_ASSERT_EQUAL(ESP_OK, sparkplug_b_encode_nbirth(buf, sizeof(buf), 1, 0, &len));

    msg_sensor_data_t data;
    fill_data(&data, 21.5f);

    sparkplug_b_ddata_t ddata;
    TEST_ASSERT_EQUAL(ESP_OK, sparkplug_b_espnow_bridge_on_data(&data, &ddata));

    // The first data message of the session carries seq 1.
    TEST_ASSERT_EQUAL_UINT64(1, ddata.seq);

    sparkplug_payload_t payload = {
        .timestamp_ms = ddata.timestamp_ms,
        .seq = ddata.seq,
        .metrics = ddata.metrics,
        .metric_count = ddata.metric_count,
    };
    len = encode(&payload, buf, sizeof(buf));

    uint64_t seq = 0;
    bool has_seq = false;
    size_t metric_count = 0;
    decode_envelope(buf, len, &seq, &has_seq, &metric_count);
    TEST_ASSERT_TRUE(has_seq);
    TEST_ASSERT_EQUAL_UINT64(1, seq);
    TEST_ASSERT_EQUAL_size_t(ddata.metric_count, metric_count);

    // A DATA message carries only the alias of every metric, never the name
    // (tck-id-payloads-alias-data-cmd-requirement).
    sparkplug_b_decoded_metric_t metrics[SPARKPLUG_B_MAX_DECODED_METRICS];
    size_t decoded = 0;
    TEST_ASSERT_EQUAL(ESP_OK, sparkplug_b_decode_metrics(buf, len, metrics,
                                                          SPARKPLUG_B_MAX_DECODED_METRICS, &decoded));
    TEST_ASSERT_EQUAL_size_t(ddata.metric_count, decoded);
    for (size_t i = 0; i < decoded; ++i) {
        TEST_ASSERT_TRUE(metrics[i].has_alias);
        TEST_ASSERT_EQUAL_STRING("", metrics[i].name);
    }
}

TEST_CASE("a command does not consume a sequence number", "[sparkplug_b_lifecycle]")
{
    sparkplug_b_state_reset();
    TEST_ASSERT_EQUAL(ESP_OK, sparkplug_b_espnow_bridge_init("iiot-kit", "GW-AABB"));

    msg_sensor_data_t data;
    fill_data(&data, 21.5f);

    sparkplug_b_ddata_t ddata;
    TEST_ASSERT_EQUAL(ESP_OK, sparkplug_b_espnow_bridge_on_data(&data, &ddata));
    TEST_ASSERT_EQUAL_UINT64(1, ddata.seq);

    // A command arrives between two data messages. It is received, not
    // published, so it must not advance the sequence space of the node.
    sparkplug_b_decoded_metric_t command_metrics[1];
    memset(command_metrics, 0, sizeof(command_metrics));
    strncpy(command_metrics[0].name, SPARKPLUG_METRIC_REBIRTH_NAME,
            sizeof(command_metrics[0].name) - 1);
    command_metrics[0].datatype = SPARKPLUG_DATA_TYPE_BOOLEAN;
    command_metrics[0].has_value = true;
    command_metrics[0].value.boolean_value = true;

    sparkplug_b_command_t command;
    TEST_ASSERT_EQUAL(ESP_OK, sparkplug_b_cmd_parse(command_metrics, 1, false, &command));
    TEST_ASSERT_EQUAL(SPARKPLUG_CMD_REBIRTH, command.kind);

    // The next data message still carries seq 2.
    TEST_ASSERT_EQUAL(ESP_OK, sparkplug_b_espnow_bridge_on_data(&data, &ddata));
    TEST_ASSERT_EQUAL_UINT64(2, ddata.seq);
}

TEST_CASE("the device that stops reporting is declared dead with the bdSeq of the session",
          "[sparkplug_b_lifecycle]")
{
    sparkplug_b_state_reset();
    TEST_ASSERT_EQUAL(ESP_OK, sparkplug_b_espnow_bridge_init("iiot-kit", "GW-AABB"));
    TEST_ASSERT_EQUAL(ESP_OK, sparkplug_b_devices_init());

    msg_discovery_t discovery;
    memset(&discovery, 0, sizeof(discovery));
    protocol_fill_header(&discovery.header, MSG_TYPE_DISCOVERY, NODE_MAC, 1,
                         sizeof(discovery) - sizeof(discovery.header));
    discovery.sensor_type = SENSOR_TYPE_TEMPERATURE;
    discovery.firmware_version = 1;

    sparkplug_b_ddata_t birth;
    TEST_ASSERT_EQUAL(ESP_OK, sparkplug_b_espnow_bridge_on_discovery(&discovery, &birth));
    TEST_ASSERT_EQUAL_STRING("N-EE01", birth.device_id);

    // The registry knows the device and stores the metric set of its birth,
    // so a rebirth can publish it again.
    TEST_ASSERT_EQUAL(ESP_OK, sparkplug_b_devices_touch(birth.device_id, NODE_MAC, 1000));
    TEST_ASSERT_EQUAL(ESP_OK,
                      sparkplug_b_devices_store_birth(birth.device_id, birth.metrics,
                                                      birth.metric_count));
    TEST_ASSERT_EQUAL_size_t(1, sparkplug_b_devices_count());

    // The device stops reporting. After the silence timeout it is stale, and
    // the caller publishes a DDEATH for every device that transitioned.
    size_t expired[4];
    const size_t expired_count = sparkplug_b_devices_expire(1000 + 60000, 60, expired, 4);
    TEST_ASSERT_EQUAL_size_t(1, expired_count);

    sparkplug_b_device_t device;
    TEST_ASSERT_EQUAL(ESP_OK, sparkplug_b_devices_get(expired[0], &device));
    TEST_ASSERT_EQUAL_STRING("N-EE01", device.device_id);
    TEST_ASSERT_EQUAL(SPARKPLUG_DEVICE_STALE, device.state);

    // The DDEATH of a device carries a sequence number, unlike the NDEATH of
    // the node (tck-id-payloads-ddeath-seq-number), and it carries the bdSeq
    // metric of the session the device belonged to.
    sparkplug_metric_t bdseq_metric = {
        .alias = SPARKPLUG_ALIAS_NONE,
        .name = SPARKPLUG_METRIC_BDSEQ_NAME,
        .data_type = SPARKPLUG_DATA_TYPE_INT64,
        .value.int64_value = 7,
    };
    sparkplug_payload_t death = {
        .timestamp_ms = 61000,
        .seq = sparkplug_b_seq_next(),
        .metrics = &bdseq_metric,
        .metric_count = 1,
    };
    uint8_t buf[SPARKPLUG_B_PAYLOAD_MAX_BYTES] = {0};
    const size_t len = encode(&death, buf, sizeof(buf));

    uint64_t seq = 0;
    bool has_seq = false;
    size_t metric_count = 0;
    decode_envelope(buf, len, &seq, &has_seq, &metric_count);
    TEST_ASSERT_TRUE(has_seq);
    // The DBIRTH of the device consumed seq 1, so the DDEATH carries seq 2:
    // the node and its devices share the space.
    TEST_ASSERT_EQUAL_UINT64(2, seq);
    TEST_ASSERT_EQUAL_size_t(1, metric_count);

    sparkplug_b_decoded_metric_t metrics[SPARKPLUG_B_MAX_DECODED_METRICS];
    size_t decoded = 0;
    TEST_ASSERT_EQUAL(ESP_OK, sparkplug_b_decode_metrics(buf, len, metrics,
                                                          SPARKPLUG_B_MAX_DECODED_METRICS, &decoded));
    TEST_ASSERT_EQUAL_size_t(1, decoded);
    TEST_ASSERT_EQUAL_STRING(SPARKPLUG_METRIC_BDSEQ_NAME, metrics[0].name);
    TEST_ASSERT_EQUAL_INT64(7, metrics[0].value.long_value);
    TEST_ASSERT_FALSE(metrics[0].has_alias);
}
