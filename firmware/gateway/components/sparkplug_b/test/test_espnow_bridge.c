// ESP-NOW → Sparkplug B bridge tests (issue #11, commit 6).
//
// These run on-device inside gateway_test.bin (Unity via esp-idf's test
// runner) alongside test_state.c and test_payload.c.
//
// RED intent (test-first):
//   - The bridge module does not exist yet; this file fails to link until
//     src/sparkplug_b_espnow_bridge.c is materialized in commit 6.
//   - Once the module lands, the bridge functions must satisfy the contract
//     documented in include/sparkplug_b_espnow_bridge.h.

#include <string.h>

#include "esp_err.h"
#include "unity.h"

#include "protocol.h"
#include "sparkplug_b_espnow_bridge.h"
#include "sparkplug_b_payload.h"
#include "sparkplug_b_state.h"

static const uint8_t NODE_MAC[6] = {0xAA, 0xBB, 0xCC, 0xDD, 0xEE, 0x01};

static void fill_data_header(msg_sensor_data_t *out, uint16_t seq)
{
    memset(out, 0, sizeof(*out));
    protocol_fill_header(&out->header, MSG_TYPE_DATA, NODE_MAC, seq,
                         sizeof(*out) - sizeof(msg_header_t));
}

TEST_CASE("device_id for known MAC follows N-<mac4hex>", "[sparkplug_b_bridge]")
{
    char buf[SPARKPLUG_B_DEVICE_ID_MAX];
    TEST_ASSERT_EQUAL(ESP_OK,
                      sparkplug_b_espnow_bridge_device_id_for(NODE_MAC, buf, sizeof(buf)));
    TEST_ASSERT_EQUAL_STRING("N-EE01", buf);
}

TEST_CASE("device_id rejects NULL MAC and short buffer", "[sparkplug_b_bridge]")
{
    char buf[SPARKPLUG_B_DEVICE_ID_MAX];
    TEST_ASSERT_EQUAL(ESP_ERR_INVALID_ARG,
                      sparkplug_b_espnow_bridge_device_id_for(NULL, buf, sizeof(buf)));
    char small[4];
    TEST_ASSERT_EQUAL(ESP_ERR_INVALID_SIZE,
                      sparkplug_b_espnow_bridge_device_id_for(NODE_MAC, small, sizeof(small)));
}

TEST_CASE("on_data TEMPERATURE single value produces typed metric + diagnostics",
          "[sparkplug_b_bridge]")
{
    TEST_ASSERT_EQUAL(ESP_OK, sparkplug_b_espnow_bridge_init("iiot-kit", "GW-AABB"));

    msg_sensor_data_t in;
    fill_data_header(&in, 42);
    in.sensor_type = SENSOR_TYPE_TEMPERATURE;
    in.value_count = 1;
    in.values[0] = 23.5f;
    in.uptime_ms = 60000;
    in.battery_pct = 87;
    in.rssi_last = -65;

    sparkplug_b_ddata_t out;
    memset(&out, 0xAB, sizeof(out));
    TEST_ASSERT_EQUAL(ESP_OK, sparkplug_b_espnow_bridge_on_data(&in, &out));

    TEST_ASSERT_EQUAL_STRING("N-EE01", out.device_id);
    TEST_ASSERT_EQUAL_UINT32(60000, (uint32_t)out.timestamp_ms);
    TEST_ASSERT_GREATER_THAN_UINT64(0, out.seq);

    /* Expected metrics (in bridge-defined order):
     *   alias 1  Inputs/Temperature  double 23.5
     *   alias 11 Diagnostics/UptimeMs int64 60000
     *   alias 12 Diagnostics/BatteryPercent float 87.0
     *   alias 13 Diagnostics/RssiLast int8 -65
     */
    TEST_ASSERT_EQUAL_size_t(4, out.metric_count);
    TEST_ASSERT_EQUAL_UINT64(SPARKPLUG_ALIAS_TEMP, out.metrics[0].alias);
    TEST_ASSERT_EQUAL_STRING("Inputs/Temperature", out.metrics[0].name);
    TEST_ASSERT_EQUAL(SPARKPLUG_DATA_TYPE_DOUBLE, out.metrics[0].data_type);
    TEST_ASSERT_EQUAL_DOUBLE(23.5, out.metrics[0].value.double_value);

    TEST_ASSERT_EQUAL_UINT64(SPARKPLUG_ALIAS_UPTIME_MS, out.metrics[1].alias);
    TEST_ASSERT_EQUAL(SPARKPLUG_DATA_TYPE_INT64, out.metrics[1].data_type);
    TEST_ASSERT_EQUAL_INT64(60000, out.metrics[1].value.int64_value);

    TEST_ASSERT_EQUAL_UINT64(SPARKPLUG_ALIAS_BATTERY, out.metrics[2].alias);
    TEST_ASSERT_EQUAL(SPARKPLUG_DATA_TYPE_FLOAT, out.metrics[2].data_type);
    TEST_ASSERT_EQUAL_FLOAT(87.0f, out.metrics[2].value.float_value);

    TEST_ASSERT_EQUAL_UINT64(SPARKPLUG_ALIAS_RSSI, out.metrics[3].alias);
    TEST_ASSERT_EQUAL(SPARKPLUG_DATA_TYPE_INT8, out.metrics[3].data_type);
    TEST_ASSERT_EQUAL_INT8(-65, out.metrics[3].value.int8_value);
}

TEST_CASE("on_data multi-value typed sensor keeps values[0] typed and extras as Custom",
          "[sparkplug_b_bridge]")
{
    TEST_ASSERT_EQUAL(ESP_OK, sparkplug_b_espnow_bridge_init("iiot-kit", "GW-AABB"));

    msg_sensor_data_t in;
    fill_data_header(&in, 7);
    in.sensor_type = SENSOR_TYPE_TEMPERATURE;
    in.value_count = 3;
    in.values[0] = 22.0f;
    in.values[1] = 22.5f;
    in.values[2] = 23.0f;
    in.uptime_ms = 0;     // diagnostics suppressed? no: bridge always includes uptime
    in.battery_pct = 0xFF; // suppressed
    in.rssi_last = -70;

    sparkplug_b_ddata_t out;
    memset(&out, 0, sizeof(out));
    TEST_ASSERT_EQUAL(ESP_OK, sparkplug_b_espnow_bridge_on_data(&in, &out));

    /* Expected: alias 1 (Temp), 7 (Custom 0), 8 (Custom 1), 13 (Rssi).
     * Uptime included (0 is a legal value), battery omitted (0xFF).
     */
    TEST_ASSERT_EQUAL_size_t(4, out.metric_count);
    TEST_ASSERT_EQUAL_UINT64(SPARKPLUG_ALIAS_TEMP, out.metrics[0].alias);
    TEST_ASSERT_EQUAL_DOUBLE(22.0, out.metrics[0].value.double_value);
    TEST_ASSERT_EQUAL_UINT64(SPARKPLUG_ALIAS_CUSTOM_0, out.metrics[1].alias);
    TEST_ASSERT_EQUAL_STRING("Inputs/Custom/0", out.metrics[1].name);
    TEST_ASSERT_EQUAL_DOUBLE(22.5, out.metrics[1].value.double_value);
    TEST_ASSERT_EQUAL_UINT64(SPARKPLUG_ALIAS_CUSTOM_1, out.metrics[2].alias);
    TEST_ASSERT_EQUAL_STRING("Inputs/Custom/1", out.metrics[2].name);
    TEST_ASSERT_EQUAL_DOUBLE(23.0, out.metrics[2].value.double_value);
    TEST_ASSERT_EQUAL_UINT64(SPARKPLUG_ALIAS_RSSI, out.metrics[3].alias);
}

TEST_CASE("on_data CUSTOM sensor maps all slots to Custom/0..3", "[sparkplug_b_bridge]")
{
    TEST_ASSERT_EQUAL(ESP_OK, sparkplug_b_espnow_bridge_init("iiot-kit", "GW-AABB"));

    msg_sensor_data_t in;
    fill_data_header(&in, 99);
    in.sensor_type = SENSOR_TYPE_CUSTOM;
    in.value_count = PROTOCOL_MAX_VALUES;  // 4
    in.values[0] = 1.1f;
    in.values[1] = 2.2f;
    in.values[2] = 3.3f;
    in.values[3] = 4.4f;
    in.uptime_ms = 1;
    in.battery_pct = 50;
    in.rssi_last = -80;

    sparkplug_b_ddata_t out;
    memset(&out, 0, sizeof(out));
    TEST_ASSERT_EQUAL(ESP_OK, sparkplug_b_espnow_bridge_on_data(&in, &out));

    /* 4 Custom + uptime + battery + rssi = 7 */
    TEST_ASSERT_EQUAL_size_t(7, out.metric_count);
    for (size_t i = 0; i < 4; ++i) {
        TEST_ASSERT_EQUAL_UINT64(SPARKPLUG_ALIAS_CUSTOM_0 + i, out.metrics[i].alias);
        char expected[24];
        snprintf(expected, sizeof(expected), "Inputs/Custom/%zu", i);
        TEST_ASSERT_EQUAL_STRING(expected, out.metrics[i].name);
        TEST_ASSERT_EQUAL(SPARKPLUG_DATA_TYPE_DOUBLE, out.metrics[i].data_type);
    }
    TEST_ASSERT_EQUAL_UINT64(SPARKPLUG_ALIAS_UPTIME_MS, out.metrics[4].alias);
    TEST_ASSERT_EQUAL_UINT64(SPARKPLUG_ALIAS_BATTERY, out.metrics[5].alias);
    TEST_ASSERT_EQUAL_UINT64(SPARKPLUG_ALIAS_RSSI, out.metrics[6].alias);
}

TEST_CASE("on_data rejects NULL pointers", "[sparkplug_b_bridge]")
{
    msg_sensor_data_t in;
    fill_data_header(&in, 1);
    sparkplug_b_ddata_t out;
    TEST_ASSERT_EQUAL(ESP_ERR_INVALID_ARG, sparkplug_b_espnow_bridge_on_data(NULL, &out));
    TEST_ASSERT_EQUAL(ESP_ERR_INVALID_ARG, sparkplug_b_espnow_bridge_on_data(&in, NULL));
}

TEST_CASE("on_discovery produces DBIRTH-shape payload with same aliases as DDATA",
          "[sparkplug_b_bridge]")
{
    TEST_ASSERT_EQUAL(ESP_OK, sparkplug_b_espnow_bridge_init("iiot-kit", "GW-AABB"));

    msg_discovery_t in;
    memset(&in, 0, sizeof(in));
    protocol_fill_header(&in.header, MSG_TYPE_DISCOVERY, NODE_MAC, 1,
                         sizeof(in) - sizeof(msg_header_t));
    in.firmware_version = 0x00010002;  // 1.2
    in.sensor_type = SENSOR_TYPE_TEMPERATURE;
    in.sleep_interval_sec = 300;
    in.battery_pct = 75;

    sparkplug_b_ddata_t out;
    memset(&out, 0, sizeof(out));
    TEST_ASSERT_EQUAL(ESP_OK, sparkplug_b_espnow_bridge_on_discovery(&in, &out));

    TEST_ASSERT_EQUAL_STRING("N-EE01", out.device_id);
    /* DBIRTH must include at least one typed metric (temperature) and the
     * diagnostics surface must reflect the discovery payload. The exact
     * metric ordering for DBIRTH matches DDATA so the Host can rely on
     * a stable shape across births and data messages. */
    TEST_ASSERT_GREATER_OR_EQUAL_size_t(3, out.metric_count);
    TEST_ASSERT_EQUAL_UINT64(SPARKPLUG_ALIAS_TEMP, out.metrics[0].alias);
    TEST_ASSERT_EQUAL_UINT64(SPARKPLUG_ALIAS_BATTERY, out.metrics[1].alias);
}

TEST_CASE("bridge on_data increments the global seq counter", "[sparkplug_b_bridge]")
{
    TEST_ASSERT_EQUAL(ESP_OK, sparkplug_b_espnow_bridge_init("iiot-kit", "GW-AABB"));
    sparkplug_b_state_reset();

    msg_sensor_data_t in;
    fill_data_header(&in, 0);
    in.sensor_type = SENSOR_TYPE_TEMPERATURE;
    in.value_count = 1;
    in.values[0] = 21.0f;
    in.uptime_ms = 1;
    in.battery_pct = 0xFF;
    in.rssi_last = 0;

    sparkplug_b_ddata_t a_out, b_out;
    memset(&a_out, 0, sizeof(a_out));
    memset(&b_out, 0, sizeof(b_out));

    TEST_ASSERT_EQUAL(ESP_OK, sparkplug_b_espnow_bridge_on_data(&in, &a_out));
    TEST_ASSERT_EQUAL(ESP_OK, sparkplug_b_espnow_bridge_on_data(&in, &b_out));
    TEST_ASSERT_EQUAL_UINT64(a_out.seq + 1, b_out.seq);
}

TEST_CASE("bridge init rejects NULL or empty identities", "[sparkplug_b_bridge]")
{
    TEST_ASSERT_EQUAL(ESP_ERR_INVALID_ARG, sparkplug_b_espnow_bridge_init(NULL, "GW-AABB"));
    TEST_ASSERT_EQUAL(ESP_ERR_INVALID_ARG, sparkplug_b_espnow_bridge_init("iiot-kit", NULL));
    TEST_ASSERT_EQUAL(ESP_ERR_INVALID_ARG, sparkplug_b_espnow_bridge_init("", "GW-AABB"));
}