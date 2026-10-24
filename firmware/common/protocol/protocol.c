#include "protocol.h"

#include <string.h>

static size_t message_size(msg_type_t type)
{
    switch (type) {
    case MSG_TYPE_DATA: return sizeof(msg_sensor_data_t);
    case MSG_TYPE_ACK: return sizeof(msg_ack_t);
    case MSG_TYPE_DISCOVERY: return sizeof(msg_discovery_t);
    case MSG_TYPE_CONFIG: return sizeof(msg_config_t);
    case MSG_TYPE_OTA_PREPARE: return sizeof(msg_ota_prepare_t);
    default: return 0;
    }
}

int protocol_serialize(const void *msg, msg_type_t type, uint8_t *buf, size_t buf_len)
{
    size_t size = message_size(type);
    if (msg == NULL || buf == NULL || size == 0 || buf_len < size || size > PROTOCOL_MAX_PAYLOAD + sizeof(msg_header_t)) {
        return -1;
    }

    const msg_header_t *header = msg;
    if (header->msg_type != (uint8_t)type || header->payload_len != size - sizeof(msg_header_t)) {
        return -1;
    }
    memcpy(buf, msg, size);
    return (int)size;
}

esp_err_t protocol_deserialize_header(const uint8_t *buf, size_t len, msg_header_t *header)
{
    if (buf == NULL || header == NULL) {
        return ESP_ERR_INVALID_ARG;
    }
    if (len < sizeof(*header)) {
        return ESP_ERR_INVALID_SIZE;
    }
    memcpy(header, buf, sizeof(*header));
    return ESP_OK;
}

esp_err_t protocol_deserialize(const uint8_t *buf, size_t len, msg_type_t type, void *msg)
{
    size_t size = message_size(type);
    if (buf == NULL || msg == NULL || size == 0) {
        return ESP_ERR_INVALID_ARG;
    }
    if (len != size || !protocol_validate(buf, len)) {
        return ESP_ERR_INVALID_SIZE;
    }
    if (buf[offsetof(msg_header_t, msg_type)] != (uint8_t)type) {
        return ESP_ERR_INVALID_ARG;
    }
    memcpy(msg, buf, size);
    return ESP_OK;
}

void protocol_fill_header(msg_header_t *header, msg_type_t type, const uint8_t *mac, uint16_t seq,
                          uint16_t payload_len)
{
    if (header == NULL || mac == NULL) {
        return;
    }
    header->version = PROTOCOL_VERSION;
    header->msg_type = (uint8_t)type;
    memcpy(header->node_mac, mac, sizeof(header->node_mac));
    header->sequence = seq;
    header->reserved = 0;
    header->payload_len = payload_len;
}

const char *protocol_msg_type_str(msg_type_t type)
{
    switch (type) {
    case MSG_TYPE_DATA: return "DATA";
    case MSG_TYPE_ACK: return "ACK";
    case MSG_TYPE_DISCOVERY: return "DISCOVERY";
    case MSG_TYPE_CONFIG: return "CONFIG";
    case MSG_TYPE_OTA_PREPARE: return "OTA_PREPARE";
    default: return "UNKNOWN";
    }
}

const char *protocol_sensor_type_str(sensor_type_t type)
{
    switch (type) {
    case SENSOR_TYPE_TEMPERATURE: return "TEMPERATURE";
    case SENSOR_TYPE_PH: return "PH";
    case SENSOR_TYPE_DO: return "DO";
    case SENSOR_TYPE_ORP: return "ORP";
    case SENSOR_TYPE_LEVEL: return "LEVEL";
    case SENSOR_TYPE_TURBIDITY: return "TURBIDITY";
    case SENSOR_TYPE_CUSTOM: return "CUSTOM";
    default: return "UNKNOWN";
    }
}

bool protocol_validate(const uint8_t *buf, size_t len)
{
    msg_header_t header;
    if (buf == NULL || len < sizeof(header)) {
        return false;
    }
    memcpy(&header, buf, sizeof(header));
    if (header.version != PROTOCOL_VERSION || message_size((msg_type_t)header.msg_type) == 0 ||
        header.payload_len != len - sizeof(header)) {
        return false;
    }
    if (header.msg_type == MSG_TYPE_DATA) {
        const size_t count_offset = offsetof(msg_sensor_data_t, value_count);
        if (len <= count_offset || buf[count_offset] < 1 || buf[count_offset] > PROTOCOL_MAX_VALUES) {
            return false;
        }
    }
    return true;
}
