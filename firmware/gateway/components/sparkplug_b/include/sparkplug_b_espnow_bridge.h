#pragma once

// ESP-NOW → Sparkplug B translation bridge (issue #11, commit 6).
//
// This module consumes the materialized ESP-NOW protocol frames from
// firmware/common/protocol/protocol.h and produces Sparkplug B payload
// builders ready to be fed into sparkplug_b_publish_ddata() (commit 7+).
//
// Design references:
//   - design.md §3.5 (Mapeo ESP-NOW → Sparkplug)
//   - design.md §3.4 (seq global, bdSeq en reconnect)
//   - design.md §3.3 (Estados por Device: UNKNOWN → ALIVE → STALE)
//
// Closed decisions (validated 2026-10-05):
//   D-B1  value_count > 1: values[0] → métrica tipada; values[1..N-1] →
//         Inputs/Custom/0..N-2 con alias CUSTOM_0..CUSTOM_3.
//   D-B2  Timestamp: esp_timer_get_time() / 1000 (monotonic). Un ESP_LOGW
//         en el primer encode indica que se publicará hasta Fase 5 (SNTP).
//   D-B3  Periodicidad: 1 DDATA por cada MSG_TYPE_DATA (1:1).
//   D-B4  seq: global del EoN-Node vía sparkplug_b_seq_next().
//
// Alias map (firmado en commit 4 + bridge commit 6):
//   0       reserved (Sparkplug B v3 spec)
//   1..6    Inputs/{Temperature,Ph,Do,Orp,Level,Turbidity}      (Double)
//   7..10   Inputs/Custom/0..3                                  (Double)
//   11      Diagnostics/UptimeMs                                (Int64)
//   12      Diagnostics/BatteryPercent                          (Float)
//   13      Diagnostics/RssiLast                                (Int8)
//
// device_id format (D2): "N-<mac4hex>" (default). Template configurable
// vía mqtt_device_id_template en NVS, fuera del scope de este commit.

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "esp_err.h"

#include "protocol.h"
#include "sparkplug_b_payload.h"

#ifdef __cplusplus
extern "C" {
#endif

/** device_id buffer size: "N-" + 4 hex chars + NUL. Ajustado a 32 para tolerar templates largos. */
#define SPARKPLUG_B_DEVICE_ID_MAX 32

/**
 * Output of a translation. The caller is responsible for passing the
 * sparkplug_payload_t inside this struct to sparkplug_b_encode_payload()
 * (and eventually to sparkplug_b_publish_ddata() in commit 7).
 *
 * The metric buffer is owned by the caller; the bridge never allocates
 * per-call. Pass a buffer large enough for the typed metric + diagnostics
 * + any custom slots used.
 */
typedef struct {
    char device_id[SPARKPLUG_B_DEVICE_ID_MAX];
    sparkplug_metric_t metrics[128];  // generous upper bound; flag present
    size_t metric_count;
    uint64_t timestamp_ms;
    uint64_t seq;
} sparkplug_b_ddata_t;

/**
 * @brief Initialize the bridge with the EoN-Node identities.
 *
 * Must be called once after sparkplug_b_start() and before any on_data /
 * on_discovery call. group_id and edge_node_id are copied internally;
 * they only affect the device_id format and are mirrored in the output
 * struct for downstream consumers.
 *
 * @param group_id      Sparkplug group_id (e.g., "iiot-kit").
 * @param edge_node_id  EoN-Node id (e.g., "GW-AABB").
 * @return ESP_OK on success, ESP_ERR_INVALID_ARG if any argument is NULL/empty.
 */
esp_err_t sparkplug_b_espnow_bridge_init(const char *group_id, const char *edge_node_id);

/**
 * @brief Translate a MSG_TYPE_DATA frame into a Sparkplug DDATA builder.
 *
 * Mapping (see design.md §3.5 + closed decisions D-B1..D-B4):
 *   - sensor_type TEMPERATURE/PH/DO/ORP/LEVEL/TURBIDITY → typed metric
 *     at alias 1..6 with values[0].
 *   - value_count > 1 → values[1..N-1] → Inputs/Custom/0..N-2 at
 *     alias CUSTOM_0..CUSTOM_3 (capped at 4).
 *   - sensor_type CUSTOM → all PROTOCOL_MAX_VALUES slots go to
 *     Inputs/Custom/0..3 (alias 7..10).
 *   - uptime_ms → Diagnostics/UptimeMs (Int64) at alias 11.
 *   - battery_pct != 0xFF → Diagnostics/BatteryPercent (Float) at alias 12.
 *   - rssi_last → Diagnostics/RssiLast (Int8) at alias 13.
 *
 * @param data  Input ESP-NOW frame (already deserialized).
 * @param out   Output builder. Must be non-NULL. Cleared on entry.
 * @return ESP_OK on success, ESP_ERR_INVALID_ARG if data or out is NULL.
 */
esp_err_t sparkplug_b_espnow_bridge_on_data(const msg_sensor_data_t *data,
                                            sparkplug_b_ddata_t *out);

/**
 * @brief Translate a MSG_TYPE_DISCOVERY frame into a Sparkplug DBIRTH builder.
 *
 * DBIRTH shape is identical to DDATA but uses the discovery payload
 * fields: firmware_version → Properties/HwVersion (UInt32-ish via Int64
 * widening; full Properties support is out of commit 6 scope).
 *
 * @param disc   Input discovery frame (already deserialized).
 * @param out     Output builder. Must be non-NULL. Cleared on entry.
 * @return ESP_OK on success, ESP_ERR_INVALID_ARG if disc or out is NULL.
 */
esp_err_t sparkplug_b_espnow_bridge_on_discovery(const msg_discovery_t *disc,
                                                sparkplug_b_ddata_t *out);

/**
 * @brief Format a device_id from a node MAC address.
 *
 * Uses the default template "N-<mac4hex>". Custom templates land in a
 * later commit (mqtt_device_id_template in NVS).
 *
 * @param mac       Node MAC (6 bytes). NULL-safe (returns ESP_ERR_INVALID_ARG).
 * @param buf       Destination buffer.
 * @param buf_len   Destination capacity (recommend >= SPARKPLUG_B_DEVICE_ID_MAX).
 * @return ESP_OK on success, ESP_ERR_INVALID_ARG / ESP_ERR_INVALID_SIZE otherwise.
 */
esp_err_t sparkplug_b_espnow_bridge_device_id_for(const uint8_t mac[6],
                                                  char *buf, size_t buf_len);

#ifdef __cplusplus
}
#endif