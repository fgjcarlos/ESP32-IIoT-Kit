#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "esp_err.h"

#define PROTOCOL_VERSION 1
#define PROTOCOL_MAX_PAYLOAD 237
#define PROTOCOL_MAX_VALUES 4

typedef enum {
    MSG_TYPE_DATA = 0x01,
    MSG_TYPE_ACK = 0x02,
    MSG_TYPE_DISCOVERY = 0x03,
    MSG_TYPE_CONFIG = 0x04,
    MSG_TYPE_OTA_PREPARE = 0x05,
} msg_type_t;

typedef enum {
    SENSOR_TYPE_TEMPERATURE = 0x01,
    SENSOR_TYPE_PH = 0x02,
    SENSOR_TYPE_DO = 0x03,
    SENSOR_TYPE_ORP = 0x04,
    SENSOR_TYPE_LEVEL = 0x05,
    SENSOR_TYPE_TURBIDITY = 0x06,
    SENSOR_TYPE_CUSTOM = 0xFF,
} sensor_type_t;

typedef struct __attribute__((packed)) {
    uint8_t version;
    uint8_t msg_type;
    uint8_t node_mac[6];
    uint16_t sequence;
    uint8_t reserved;
    uint16_t payload_len;
} msg_header_t;

_Static_assert(sizeof(msg_header_t) == 13, "Header must be 13 bytes");

typedef struct __attribute__((packed)) {
    msg_header_t header;
    uint8_t sensor_type;
    uint8_t value_count;
    float values[PROTOCOL_MAX_VALUES];
    uint32_t uptime_ms;
    uint8_t battery_pct;
    int8_t rssi_last;
} msg_sensor_data_t;

_Static_assert(sizeof(msg_sensor_data_t) == 37, "Sensor data must be 37 bytes");

typedef struct __attribute__((packed)) {
    msg_header_t header;
    uint16_t ack_sequence;
    uint32_t gateway_timestamp;
    uint8_t gateway_channel;
} msg_ack_t;

_Static_assert(sizeof(msg_ack_t) == 20, "ACK must be 20 bytes");

typedef struct __attribute__((packed)) {
    msg_header_t header;
    uint32_t firmware_version;
    uint8_t sensor_type;
    uint16_t sleep_interval_sec;
    uint8_t battery_pct;
} msg_discovery_t;

_Static_assert(sizeof(msg_discovery_t) == 21, "Discovery must be 21 bytes");

typedef struct __attribute__((packed)) {
    msg_header_t header;
    uint16_t new_sleep_interval;
    uint8_t flags;
} msg_config_t;

_Static_assert(sizeof(msg_config_t) == 16, "Config must be 16 bytes");

typedef struct __attribute__((packed)) {
    msg_header_t header;
    uint8_t ap_ssid[32];
    uint8_t ap_ssid_len;
    uint8_t ap_password[64];
    uint8_t ap_password_len;
    uint8_t ap_channel;
} msg_ota_prepare_t;

_Static_assert(sizeof(msg_ota_prepare_t) == 112, "OTA prepare must be 112 bytes");

/**
 * @brief Serialize a protocol message into its packed wire representation.
 * @param msg Message structure whose header and payload are already populated.
 * @param type Message type, used to determine the structure size.
 * @param buf Destination buffer.
 * @param buf_len Destination capacity in bytes.
 * @return Serialized byte count, or -1 for an invalid argument/type or short buffer.
 */
int protocol_serialize(const void *msg, msg_type_t type, uint8_t *buf, size_t buf_len);

/**
 * @brief Copy and validate the common message header.
 * @param buf Input wire buffer.
 * @param len Input length in bytes.
 * @param header Destination header.
 * @return ESP_OK on success, otherwise an ESP_ERR_* code.
 */
esp_err_t protocol_deserialize_header(const uint8_t *buf, size_t len, msg_header_t *header);

/**
 * @brief Deserialize a complete message of the requested type.
 * @param buf Input wire buffer.
 * @param len Input length in bytes.
 * @param type Expected message type.
 * @param msg Destination message structure.
 * @return ESP_OK on success, otherwise an ESP_ERR_* code.
 */
esp_err_t protocol_deserialize(const uint8_t *buf, size_t len, msg_type_t type, void *msg);

/**
 * @brief Populate the common protocol header.
 * @param header Header to populate.
 * @param type Message type.
 * @param mac Source node MAC address (six bytes).
 * @param seq Sequence number.
 * @param payload_len Payload length in bytes, excluding the header.
 */
void protocol_fill_header(msg_header_t *header, msg_type_t type, const uint8_t *mac, uint16_t seq,
                          uint16_t payload_len);

/** @brief Return the printable name for a message type, or "UNKNOWN". */
const char *protocol_msg_type_str(msg_type_t type);

/** @brief Return the printable name for a sensor type, or "UNKNOWN". */
const char *protocol_sensor_type_str(sensor_type_t type);

/**
 * @brief Validate a complete protocol frame against the common header rules.
 * @param buf Input wire buffer.
 * @param len Input length in bytes.
 * @return true when the frame is valid, otherwise false.
 */
bool protocol_validate(const uint8_t *buf, size_t len);
