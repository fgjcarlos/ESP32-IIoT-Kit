#include "sparkplug_b_cmd.h"

#include <string.h>

#include "esp_log.h"

#include "protocol.h"

#define TAG "sparkplug_b_cmd"

static bool metric_is(const sparkplug_b_decoded_metric_t *m, const char *name)
{
    return m->name[0] != '\0' && strcmp(m->name, name) == 0;
}

// A control metric is only actionable when it carries a Boolean true. A
// "Node Control/Rebirth" set to false is the value every NBIRTH carries, so
// accepting false would turn a birth into a command.
static bool metric_flag(const sparkplug_b_decoded_metric_t *m, bool *enabled)
{
    if (!m->has_value || m->datatype != SPARKPLUG_DATA_TYPE_BOOLEAN) {
        return false;
    }
    *enabled = m->value.boolean_value;
    return true;
}

// Extract a whole number of seconds from a numeric metric. The value type
// varies with the datatype the Host chose, so every numeric arm is accepted.
static bool metric_seconds(const sparkplug_b_decoded_metric_t *m, uint16_t *seconds)
{
    if (!m->has_value) {
        return false;
    }

    uint64_t raw = 0;
    switch (m->datatype) {
    case SPARKPLUG_DATA_TYPE_INT64:
    case SPARKPLUG_DATA_TYPE_UINT64:
        // The decoder stores the int64 arm reinterpreted as signed; a sleep
        // interval is never negative, so reject anything below zero.
        if (m->value.long_value < 0) {
            return false;
        }
        raw = (uint64_t)m->value.long_value;
        break;
    case SPARKPLUG_DATA_TYPE_INT8:
    case SPARKPLUG_DATA_TYPE_UINT8:
        raw = m->value.int_value;
        break;
    case SPARKPLUG_DATA_TYPE_DOUBLE:
        if (m->value.double_value < 0.0) {
            return false;
        }
        raw = (uint64_t)m->value.double_value;
        break;
    case SPARKPLUG_DATA_TYPE_FLOAT:
        if (m->value.float_value < 0.0f) {
            return false;
        }
        raw = (uint64_t)m->value.float_value;
        break;
    default:
        return false;
    }

    if (raw > UINT16_MAX) {
        return false;
    }
    *seconds = (uint16_t)raw;
    return true;
}

esp_err_t sparkplug_b_cmd_parse(const sparkplug_b_decoded_metric_t *metrics,
                                 size_t metric_count,
                                 bool device_command,
                                 sparkplug_b_command_t *out)
{
    if (metrics == NULL || out == NULL) {
        return ESP_ERR_INVALID_ARG;
    }

    memset(out, 0, sizeof(*out));
    out->kind = SPARKPLUG_CMD_NONE;

    bool saw_metric = false;

    for (size_t i = 0; i < metric_count; ++i) {
        const sparkplug_b_decoded_metric_t *m = &metrics[i];
        saw_metric = true;
        bool enabled = false;

        if (!device_command) {
            // NCMD: node scoped control metrics.
            if (metric_is(m, SPARKPLUG_METRIC_REBIRTH_NAME)) {
                out->kind = (metric_flag(m, &enabled) && enabled)
                    ? SPARKPLUG_CMD_REBIRTH : SPARKPLUG_CMD_UNSUPPORTED;
                return ESP_OK;
            }
            if (metric_is(m, SPARKPLUG_METRIC_REBOOT_NAME)) {
                out->kind = (metric_flag(m, &enabled) && enabled)
                    ? SPARKPLUG_CMD_NODE_REBOOT : SPARKPLUG_CMD_UNSUPPORTED;
                return ESP_OK;
            }
            continue;
        }

        // DCMD: device scoped control metrics.
        if (metric_is(m, SPARKPLUG_METRIC_DEVICE_REBIRTH_NAME)) {
            out->kind = (metric_flag(m, &enabled) && enabled)
                ? SPARKPLUG_CMD_DEVICE_REBIRTH : SPARKPLUG_CMD_UNSUPPORTED;
            return ESP_OK;
        }
        if (metric_is(m, SPARKPLUG_METRIC_DEVICE_REBOOT_NAME)) {
            out->kind = (metric_flag(m, &enabled) && enabled)
                ? SPARKPLUG_CMD_DEVICE_REBOOT : SPARKPLUG_CMD_UNSUPPORTED;
            return ESP_OK;
        }

        uint16_t seconds = 0;
        if (metric_seconds(m, &seconds)) {
            out->kind = SPARKPLUG_CMD_SET_SLEEP;
            out->sleep_interval_s = seconds;
            return ESP_OK;
        }
    }

    // A payload with metrics this gateway does not implement is reported as
    // unsupported rather than silently ignored.
    out->kind = saw_metric ? SPARKPLUG_CMD_UNSUPPORTED : SPARKPLUG_CMD_NONE;
    return ESP_OK;
}

const char *sparkplug_b_cmd_kind_str(sparkplug_b_cmd_kind_t kind)
{
    switch (kind) {
    case SPARKPLUG_CMD_NONE:           return "NONE";
    case SPARKPLUG_CMD_REBIRTH:        return "REBIRTH";
    case SPARKPLUG_CMD_NODE_REBOOT:    return "NODE_REBOOT";
    case SPARKPLUG_CMD_DEVICE_REBIRTH: return "DEVICE_REBIRTH";
    case SPARKPLUG_CMD_DEVICE_REBOOT:  return "DEVICE_REBOOT";
    case SPARKPLUG_CMD_SET_SLEEP:      return "SET_SLEEP";
    case SPARKPLUG_CMD_UNSUPPORTED:    return "UNSUPPORTED";
    default:                           return "UNKNOWN";
    }
}

esp_err_t sparkplug_b_cmd_to_espnow(const sparkplug_b_command_t *cmd,
                                     const uint8_t node_mac[6],
                                     uint16_t sequence,
                                     uint8_t *out, size_t out_size,
                                     size_t *out_len)
{
    if (cmd == NULL || node_mac == NULL || out == NULL || out_len == NULL) {
        return ESP_ERR_INVALID_ARG;
    }

    uint8_t flags = 0;
    uint16_t sleep_interval = 0;

    switch (cmd->kind) {
    case SPARKPLUG_CMD_DEVICE_REBOOT:
        flags = MSG_CONFIG_FLAG_REBOOT;
        break;
    case SPARKPLUG_CMD_SET_SLEEP:
        flags = MSG_CONFIG_FLAG_SLEEP_INTERVAL;
        sleep_interval = cmd->sleep_interval_s;
        break;
    default:
        // Node scoped Sparkplug operations have no ESP-NOW equivalent.
        ESP_LOGW(TAG, "Command %s has no ESP-NOW equivalent",
                 sparkplug_b_cmd_kind_str(cmd->kind));
        return ESP_ERR_NOT_SUPPORTED;
    }

    msg_config_t msg = {0};
    msg.new_sleep_interval = sleep_interval;
    msg.flags = flags;
    protocol_fill_header(&msg.header, MSG_TYPE_CONFIG, node_mac, sequence,
                         (uint16_t)(sizeof(msg_config_t) - sizeof(msg_header_t)));

    int written = protocol_serialize(&msg, MSG_TYPE_CONFIG, out, out_size);
    if (written < 0) {
        ESP_LOGE(TAG, "Failed to serialize config frame");
        return ESP_FAIL;
    }
    *out_len = (size_t)written;
    return ESP_OK;
}
