#include "protocol.h"

#include <string.h>

#include "unity.h"

TEST_CASE("protocol header round-trip", "[protocol]")
{
    const uint8_t mac[6] = {0x02, 0x11, 0x22, 0x33, 0x44, 0x55};
    msg_sensor_data_t source = {0};
    protocol_fill_header(&source.header, MSG_TYPE_DATA, mac, 0x1234, sizeof(source) - sizeof(source.header));

    uint8_t buf[sizeof(source)];
    TEST_ASSERT_EQUAL_INT(sizeof(source), protocol_serialize(&source, MSG_TYPE_DATA, buf, sizeof(buf)));

    msg_header_t actual;
    TEST_ASSERT_EQUAL(ESP_OK, protocol_deserialize_header(buf, sizeof(buf), &actual));
    TEST_ASSERT_EQUAL_UINT8(source.header.version, actual.version);
    TEST_ASSERT_EQUAL_UINT8(source.header.msg_type, actual.msg_type);
    TEST_ASSERT_EQUAL_UINT8_ARRAY(source.header.node_mac, actual.node_mac, sizeof(actual.node_mac));
    TEST_ASSERT_EQUAL_UINT16(source.header.sequence, actual.sequence);
    TEST_ASSERT_EQUAL_UINT8(source.header.reserved, actual.reserved);
    TEST_ASSERT_EQUAL_UINT16(source.header.payload_len, actual.payload_len);
}

TEST_CASE("protocol sensor data round-trip", "[protocol]")
{
    const uint8_t mac[6] = {1, 2, 3, 4, 5, 6};
    msg_sensor_data_t source = {0};
    protocol_fill_header(&source.header, MSG_TYPE_DATA, mac, 7, sizeof(source) - sizeof(source.header));
    source.sensor_type = SENSOR_TYPE_TEMPERATURE;
    source.value_count = 1;
    source.values[0] = 23.5f;
    source.uptime_ms = 12345;
    source.battery_pct = 80;
    source.rssi_last = -65;

    uint8_t buf[sizeof(source)];
    TEST_ASSERT_EQUAL_INT(sizeof(source), protocol_serialize(&source, MSG_TYPE_DATA, buf, sizeof(buf)));

    msg_sensor_data_t actual;
    TEST_ASSERT_EQUAL(ESP_OK, protocol_deserialize(buf, sizeof(buf), MSG_TYPE_DATA, &actual));
    TEST_ASSERT_EQUAL_UINT8(SENSOR_TYPE_TEMPERATURE, actual.sensor_type);
    TEST_ASSERT_EQUAL_UINT8(1, actual.value_count);
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 23.5f, actual.values[0]);
    TEST_ASSERT_EQUAL_UINT32(12345, actual.uptime_ms);
    TEST_ASSERT_EQUAL_UINT8(80, actual.battery_pct);
    TEST_ASSERT_EQUAL_INT8(-65, actual.rssi_last);
}

TEST_CASE("protocol validate checks version, length, and valid frame", "[protocol]")
{
    const uint8_t mac[6] = {0};
    msg_sensor_data_t source = {0};
    protocol_fill_header(&source.header, MSG_TYPE_DATA, mac, 1, sizeof(source) - sizeof(source.header));
    source.value_count = 1;

    uint8_t buf[sizeof(source)];
    TEST_ASSERT_EQUAL_INT(sizeof(source), protocol_serialize(&source, MSG_TYPE_DATA, buf, sizeof(buf)));
    TEST_ASSERT_TRUE(protocol_validate(buf, sizeof(buf)));

    uint8_t invalid_version[sizeof(source)];
    memcpy(invalid_version, buf, sizeof(buf));
    invalid_version[offsetof(msg_header_t, version)] = PROTOCOL_VERSION + 1;
    TEST_ASSERT_FALSE(protocol_validate(invalid_version, sizeof(invalid_version)));
    TEST_ASSERT_FALSE(protocol_validate(buf, sizeof(msg_header_t) - 1));
}

TEST_CASE("protocol message type strings", "[protocol]")
{
    TEST_ASSERT_EQUAL_STRING("DATA", protocol_msg_type_str(MSG_TYPE_DATA));
    TEST_ASSERT_EQUAL_STRING("ACK", protocol_msg_type_str(MSG_TYPE_ACK));
    TEST_ASSERT_EQUAL_STRING("DISCOVERY", protocol_msg_type_str(MSG_TYPE_DISCOVERY));
    TEST_ASSERT_EQUAL_STRING("CONFIG", protocol_msg_type_str(MSG_TYPE_CONFIG));
    TEST_ASSERT_EQUAL_STRING("OTA_PREPARE", protocol_msg_type_str(MSG_TYPE_OTA_PREPARE));
    TEST_ASSERT_EQUAL_STRING("UNKNOWN", protocol_msg_type_str((msg_type_t)0xFE));
}

TEST_CASE("protocol sensor type strings", "[protocol]")
{
    TEST_ASSERT_EQUAL_STRING("TEMPERATURE", protocol_sensor_type_str(SENSOR_TYPE_TEMPERATURE));
    TEST_ASSERT_EQUAL_STRING("PH", protocol_sensor_type_str(SENSOR_TYPE_PH));
    TEST_ASSERT_EQUAL_STRING("DO", protocol_sensor_type_str(SENSOR_TYPE_DO));
    TEST_ASSERT_EQUAL_STRING("ORP", protocol_sensor_type_str(SENSOR_TYPE_ORP));
    TEST_ASSERT_EQUAL_STRING("LEVEL", protocol_sensor_type_str(SENSOR_TYPE_LEVEL));
    TEST_ASSERT_EQUAL_STRING("TURBIDITY", protocol_sensor_type_str(SENSOR_TYPE_TURBIDITY));
    TEST_ASSERT_EQUAL_STRING("CUSTOM", protocol_sensor_type_str(SENSOR_TYPE_CUSTOM));
    TEST_ASSERT_EQUAL_STRING("UNKNOWN", protocol_sensor_type_str((sensor_type_t)0xFE));
}

TEST_CASE("protocol OTA prepare round-trip", "[protocol]")
{
    const uint8_t mac[6] = {6, 5, 4, 3, 2, 1};
    msg_ota_prepare_t source = {0};
    protocol_fill_header(&source.header, MSG_TYPE_OTA_PREPARE, mac, 9,
                         sizeof(source) - sizeof(source.header));
    memcpy(source.ap_ssid, "ESP32-AP", sizeof("ESP32-AP") - 1);
    source.ap_ssid_len = sizeof("ESP32-AP") - 1;
    memcpy(source.ap_password, "secret123", sizeof("secret123") - 1);
    source.ap_password_len = sizeof("secret123") - 1;
    source.ap_channel = 6;

    uint8_t buf[sizeof(source)];
    TEST_ASSERT_EQUAL_INT(sizeof(source), protocol_serialize(&source, MSG_TYPE_OTA_PREPARE, buf, sizeof(buf)));
    msg_ota_prepare_t actual;
    TEST_ASSERT_EQUAL(ESP_OK, protocol_deserialize(buf, sizeof(buf), MSG_TYPE_OTA_PREPARE, &actual));
    TEST_ASSERT_EQUAL_UINT8(source.ap_ssid_len, actual.ap_ssid_len);
    TEST_ASSERT_EQUAL_UINT8_ARRAY(source.ap_ssid, actual.ap_ssid, sizeof(source.ap_ssid));
    TEST_ASSERT_EQUAL_UINT8(source.ap_password_len, actual.ap_password_len);
    TEST_ASSERT_EQUAL_UINT8_ARRAY(source.ap_password, actual.ap_password, sizeof(source.ap_password));
    TEST_ASSERT_EQUAL_UINT8(6, actual.ap_channel);
}
