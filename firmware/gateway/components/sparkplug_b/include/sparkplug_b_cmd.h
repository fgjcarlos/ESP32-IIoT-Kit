#pragma once

// NCMD / DCMD interpretation (issue #11, commit 7).
//
// Sparkplug B v3.0.0 carries command payloads as a Protobuf Payload with a
// timestamp and the metrics to act on. This module turns those metrics into
// a normalized command and, where the ESP-NOW protocol can express it, into
// an ESP-NOW config frame.
//
// Design reference: odd/tasks/issue-11-sparkplug-b/design.md §3.6.
//
// Closed decisions (validated 2026-10-05):
//   D-C1  NCMD "Node Control/Rebirth" = true  -> republish NBIRTH + DBIRTHs.
//   D-C2  NCMD "Node Control/Reboot"  = true  -> reboot the gateway.
//   D-C3  DCMD "Device Control/Rebirth" = true -> republish that DBIRTH.
//   D-C4  DCMD "Device Control/Reboot"  = true -> ESP-NOW CONFIG, reboot bit.
//   D-C5  DCMD with a numeric metric           -> ESP-NOW CONFIG, new sleep
//         interval. The ESP-NOW v1.0 protocol has no sensor-write message,
//         so a write is modelled as a configuration change.
//   D-C6  Anything else -> log a warning and discard. The device stays ALIVE:
//         an invalid command is not a death certificate.

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "esp_err.h"

#include "sparkplug_b_payload.h"

#ifdef __cplusplus
extern "C" {
#endif

/** Maximum size of the ESP-NOW frame produced by the command translator. */
#define SPARKPLUG_B_CMD_FRAME_MAX 32

/** Metric name that requests a device rebirth through a DCMD. */
#define SPARKPLUG_METRIC_DEVICE_REBIRTH_NAME "Device Control/Rebirth"

/** Normalized command extracted from an NCMD or DCMD payload. */
typedef enum {
    SPARKPLUG_CMD_NONE = 0,
    /** NCMD: republish NBIRTH and the DBIRTH of every known device. */
    SPARKPLUG_CMD_REBIRTH,
    /** NCMD: reboot the gateway. */
    SPARKPLUG_CMD_NODE_REBOOT,
    /** DCMD: republish the DBIRTH of the addressed device. */
    SPARKPLUG_CMD_DEVICE_REBIRTH,
    /** DCMD: reboot the addressed node. */
    SPARKPLUG_CMD_DEVICE_REBOOT,
    /** DCMD: change the sleep interval of the addressed node. */
    SPARKPLUG_CMD_SET_SLEEP,
    /** The payload did not carry a command this gateway implements. */
    SPARKPLUG_CMD_UNSUPPORTED,
} sparkplug_b_cmd_kind_t;

/** A parsed command plus the arguments needed to act on it. */
typedef struct {
    sparkplug_b_cmd_kind_t kind;
    uint16_t sleep_interval_s;
} sparkplug_b_command_t;

/**
 * @brief Interpret the metrics of an inbound command payload.
 *
 * @param metrics        Decoded metrics from sparkplug_b_decode_metrics().
 * @param metric_count   Number of metrics.
 * @param device_command true for a DCMD (device scoped), false for an NCMD
 *                        (node scoped).
 * @param out            Parsed command. Cleared on entry.
 * @return ESP_OK on success, ESP_ERR_INVALID_ARG if metrics or out is NULL.
 */
esp_err_t sparkplug_b_cmd_parse(const sparkplug_b_decoded_metric_t *metrics,
                                 size_t metric_count,
                                 bool device_command,
                                 sparkplug_b_command_t *out);

/** @return Human-readable name of a command kind, for logs. */
const char *sparkplug_b_cmd_kind_str(sparkplug_b_cmd_kind_t kind);

/**
 * @brief Translate a command into an ESP-NOW config frame.
 *
 * Only the commands the ESP-NOW v1.0 protocol can carry are translated:
 * a device reboot and a sleep-interval change. The others are node-scoped
 * Sparkplug operations with no ESP-NOW equivalent.
 *
 * @param cmd        Parsed command.
 * @param node_mac   MAC of the addressed node (6 bytes).
 * @param sequence   Sequence number for the ESP-NOW header.
 * @param out        Destination buffer (>= SPARKPLUG_B_CMD_FRAME_MAX).
 * @param out_size   Destination capacity in bytes.
 * @param out_len    Encoded length on success.
 * @return ESP_OK on success, ESP_ERR_INVALID_ARG on bad arguments,
 *         ESP_ERR_NOT_SUPPORTED when the command has no ESP-NOW equivalent.
 */
esp_err_t sparkplug_b_cmd_to_espnow(const sparkplug_b_command_t *cmd,
                                     const uint8_t node_mac[6],
                                     uint16_t sequence,
                                     uint8_t *out, size_t out_size,
                                     size_t *out_len);

#ifdef __cplusplus
}
#endif
