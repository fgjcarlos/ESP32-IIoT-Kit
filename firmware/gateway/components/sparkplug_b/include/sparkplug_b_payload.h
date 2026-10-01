// Sparkplug B payload encode/decode helpers built on nanopb.
//
// The wire format is org.eclipse.tahu.protobuf.Payload (vendored proto).
// This header wraps nanopb pb_encode with type-safe inputs that match the
// IIoT gateway metric model from issue #11 design §3.5.
//
// All metric names and aliases are fixed by design. Callers pass typed
// values (double, float, int64_t, int8_t) and the helpers construct the
// protobuf Metric field correctly.

#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

/** Maximum encoded Sparkplug B Payload size — 237 B MTU with 1 KB safety margin. */
#define SPARKPLUG_B_PAYLOAD_MAX_BYTES 1024

/** Aliases reserved by the IIoT gateway (issue #11 design §3.5). */
typedef enum {
    SPARKPLUG_ALIAS_RESERVED   = 0,   // Reserved by Sparkplug B v3 spec
    SPARKPLUG_ALIAS_TEMP       = 1,
    SPARKPLUG_ALIAS_PH         = 2,
    SPARKPLUG_ALIAS_DO         = 3,
    SPARKPLUG_ALIAS_ORP        = 4,
    SPARKPLUG_ALIAS_LEVEL      = 5,
    SPARKPLUG_ALIAS_TURBIDITY  = 6,
    SPARKPLUG_ALIAS_CUSTOM_0   = 7,
    SPARKPLUG_ALIAS_CUSTOM_1   = 8,
    SPARKPLUG_ALIAS_CUSTOM_2   = 9,
    SPARKPLUG_ALIAS_CUSTOM_3   = 10,
    SPARKPLUG_ALIAS_UPTIME_MS  = 11,
    SPARKPLUG_ALIAS_BATTERY    = 12,
    SPARKPLUG_ALIAS_RSSI       = 13,
} sparkplug_alias_t;

/** Tag for the JSON-style metric name strings published on NBIRTH/DBIRTH. */
typedef enum {
    SPARKPLUG_DATA_TYPE_DOUBLE = 10,
    SPARKPLUG_DATA_TYPE_FLOAT  = 9,
    SPARKPLUG_DATA_TYPE_INT64  = 4,
    SPARKPLUG_DATA_TYPE_INT8   = 1,
    SPARKPLUG_DATA_TYPE_UINT8  = 5,
} sparkplug_data_type_t;

/** Single metric value pair. The union narrows by data_type. */
typedef struct {
    uint64_t alias;
    const char *name;          /* NULL on DDATA (aliases only); required on NBIRTH/DBIRTH */
    sparkplug_data_type_t data_type;
    union {
        double double_value;
        float  float_value;
        int64_t int64_value;
        int8_t  int8_value;
        uint8_t uint8_value;
    } value;
} sparkplug_metric_t;

/** Builder input for encode_payload. */
typedef struct {
    uint64_t timestamp_ms;
    uint64_t seq;
    const sparkplug_metric_t *metrics;
    size_t metric_count;
} sparkplug_payload_t;

/**
 * @brief Encode a Sparkplug B Payload into a binary buffer.
 *
 * @param payload     Builder input.
 * @param out_buf     Destination buffer (>= SPARKPLUG_B_PAYLOAD_MAX_BYTES).
 * @param out_buf_len Destination capacity (bytes).
 * @param out_len     Encoded length on success.
 * @return ESP_OK on success, ESP_ERR_INVALID_ARG / ESP_ERR_INVALID_SIZE otherwise.
 */
esp_err_t sparkplug_b_encode_payload(const sparkplug_payload_t *payload,
                                     uint8_t *out_buf, size_t out_buf_len,
                                     size_t *out_len);

/**
 * @brief Decode a Sparkplug B Payload from a binary buffer.
 *
 * Commit 4 only supports basic structural decode — the metric count is
 * exposed but individual field values are not surfaced. The MQTT client
 * (commit 5) and the conformance harness (commit 11) build a richer
 * decoder shim that captures scalar/string fields via nanopb callbacks.
 *
 * @param buf Source buffer.
 * @param len Source length (bytes).
 * @return ESP_OK on success, ESP_ERR_INVALID_ARG / ESP_ERR_INVALID_SIZE otherwise.
 */
esp_err_t sparkplug_b_decode_payload(const uint8_t *buf, size_t len);

/** @return Number of metrics decoded by the last sparkplug_b_decode_payload call. */
size_t sparkplug_b_payload_metric_count(void);

#ifdef __cplusplus
}
#endif