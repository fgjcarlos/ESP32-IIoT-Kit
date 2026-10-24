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

/**
 * Sparkplug B DataType values (subset used by the gateway).
 *
 * Values mirror org.eclipse.tahu.protobuf.DataType from the vendored
 * sparkplug_b.proto. The datatype is what a Host Application uses to
 * interpret the metric value, so it must be sent verbatim on the wire.
 */
typedef enum {
    SPARKPLUG_DATA_TYPE_INT8    = 1,
    SPARKPLUG_DATA_TYPE_INT64   = 4,
    SPARKPLUG_DATA_TYPE_UINT8   = 5,
    SPARKPLUG_DATA_TYPE_UINT64  = 8,
    SPARKPLUG_DATA_TYPE_FLOAT   = 9,
    SPARKPLUG_DATA_TYPE_DOUBLE  = 10,
    SPARKPLUG_DATA_TYPE_BOOLEAN = 11,
    SPARKPLUG_DATA_TYPE_STRING  = 12,
} sparkplug_data_type_t;

/**
 * Metric name for the birth/death sequence number.
 *
 * Sparkplug B v3.0.0 requires this metric in both NBIRTH and NDEATH so a
 * Host Application can correlate a death certificate with the birth that
 * opened the session.
 */
#define SPARKPLUG_METRIC_BDSEQ_NAME "bdSeq"

/**
 * Metric name for the rebirth request.
 *
 * Sparkplug B v3.0.0 requires every NBIRTH to carry this metric with
 * datatype Boolean and value false. A Host Application flips it to true in
 * an NCMD to request a fresh NBIRTH plus the DBIRTHs of every device.
 */
#define SPARKPLUG_METRIC_REBIRTH_NAME "Node Control/Rebirth"

/** Metric name that requests a reboot of the EoN Node. */
#define SPARKPLUG_METRIC_REBOOT_NAME "Node Control/Reboot"

/** Metric name that requests a reboot of a single Device. */
#define SPARKPLUG_METRIC_DEVICE_REBOOT_NAME "Device Control/Reboot"

/** Maximum metric name length accepted by the decoder (bytes, excluding NUL). */
#define SPARKPLUG_B_METRIC_NAME_MAX 48

/** Maximum number of metrics the decoder surfaces (matches the nanopb option). */
#define SPARKPLUG_B_MAX_DECODED_METRICS 16

/**
 * Sentinel meaning "this metric carries no alias".
 *
 * Sparkplug B v3.0.0 forbids aliasing the bdSeq and Node Control/Rebirth
 * metrics, and the reference Tahu encoder leaves the alias empty for both.
 * Alias 0 stays reserved for that reason.
 */
#define SPARKPLUG_ALIAS_NONE UINT64_MAX

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
        bool   boolean_value;
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
 * @brief Encode the node birth payload.
 *
 * Sparkplug B v3.0.0 requires the NBIRTH to carry the bdSeq metric and the
 * Node Control/Rebirth metric (datatype Boolean, value false). Neither of
 * them may carry an alias, because a Host Application has to be able to
 * request a rebirth without knowing the alias map.
 *
 * The birth opens the sequence of the session, so the encoded payload carries
 * sequence number 0.
 *
 * @param out_buf     Destination buffer (>= SPARKPLUG_B_PAYLOAD_MAX_BYTES).
 * @param out_buf_len Destination capacity (bytes).
 * @param bd_seq      Birth/death sequence number of the session.
 * @param uptime_ms   Node uptime, published as Inputs/UptimeMs.
 * @param out_len     Encoded length on success.
 * @return ESP_OK on success, ESP_ERR_INVALID_ARG / ESP_ERR_INVALID_SIZE otherwise.
 */
esp_err_t sparkplug_b_encode_nbirth(uint8_t *out_buf, size_t out_buf_len,
                                    uint64_t bd_seq, uint64_t uptime_ms,
                                    size_t *out_len);

/**
 * @brief Encode an NDEATH death certificate payload.
 *
 * Sparkplug B v3.0.0 requires the death certificate to carry a single metric
 * (bdSeq, datatype INT64) so a Host Application can correlate it with the
 * NBIRTH that opened the session, and to carry NO sequence number. That last
 * rule is why this has its own encoder instead of reusing
 * sparkplug_b_encode_payload(), which always emits a sequence number.
 *
 * @param out_buf      Destination buffer (>= SPARKPLUG_B_PAYLOAD_MAX_BYTES).
 * @param out_buf_len  Destination capacity (bytes).
 * @param bd_seq       Birth/death sequence number of the session.
 * @param timestamp_ms Timestamp of the certificate.
 * @param out_len      Encoded length on success.
 * @return ESP_OK on success, ESP_ERR_INVALID_ARG / ESP_ERR_INVALID_SIZE otherwise.
 */
esp_err_t sparkplug_b_encode_death(uint8_t *out_buf, size_t out_buf_len,
                                    uint64_t bd_seq, uint64_t timestamp_ms,
                                    size_t *out_len);

/**
 * @brief Decode a Sparkplug B Payload from a binary buffer.
 *
 * Structural decode only — the metric count is exposed but individual field
 * values are not surfaced. Use sparkplug_b_decode_metrics() when the metric
 * names and values matter (command handling).
 *
 * @param buf Source buffer.
 * @param len Source length (bytes).
 * @return ESP_OK on success, ESP_ERR_INVALID_ARG / ESP_ERR_INVALID_SIZE otherwise.
 */
esp_err_t sparkplug_b_decode_payload(const uint8_t *buf, size_t len);

/** @return Number of metrics decoded by the last sparkplug_b_decode_payload call. */
size_t sparkplug_b_payload_metric_count(void);

/**
 * @brief A metric decoded out of an inbound Sparkplug B Payload.
 *
 * Only the fields actually present on the wire are populated; `has_value` and
 * `has_alias` tell the caller which ones to trust.
 */
typedef struct {
    char     name[SPARKPLUG_B_METRIC_NAME_MAX];
    uint32_t datatype;
    bool     has_alias;
    uint64_t alias;
    bool     has_value;
    union {
        bool    boolean_value;
        int64_t long_value;
        double  double_value;
        float   float_value;
        uint32_t int_value;
        char    string_value[SPARKPLUG_B_METRIC_NAME_MAX];
    } value;
} sparkplug_b_decoded_metric_t;

/**
 * @brief Decode the metrics of an inbound Sparkplug B Payload.
 *
 * The vendored Tahu schema declares every Payload.Metric field as a
 * nanopb callback, so the metric names and values are only reachable by
 * installing decode callbacks. This is the single place that does it, so the
 * command path and any future Host-side tooling share one implementation.
 *
 * Thread-safety: the decoder uses a static scratch buffer, so it is not
 * re-entrant. The MQTT event task is the only caller in the gateway.
 *
 * @param buf       Source buffer.
 * @param len       Source length (bytes).
 * @param out       Destination array (>= SPARKPLUG_B_MAX_DECODED_METRICS entries).
 * @param max_count Capacity of `out`.
 * @param out_count Number of metrics written to `out` (may exceed max_count
 *                  only if the caller passed a smaller array; in that case the
 *                  surplus metrics are dropped).
 * @return ESP_OK on success, ESP_ERR_INVALID_ARG / ESP_ERR_INVALID_SIZE /
 *         ESP_ERR_INVALID_VERSION on failure.
 */
esp_err_t sparkplug_b_decode_metrics(const uint8_t *buf, size_t len,
                                     sparkplug_b_decoded_metric_t *out,
                                     size_t max_count,
                                     size_t *out_count);

#ifdef __cplusplus
}
#endif