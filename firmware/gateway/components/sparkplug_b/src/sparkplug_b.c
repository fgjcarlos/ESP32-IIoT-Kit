// Sparkplug B EoN-Node entry points (commit 4).
//
// Commit 4 wires the proto + codegen + payload encode/decode but does NOT
// yet publish anything to an MQTT broker. The state machine, the MQTT
// client, the TLS configuration and the NVS persistence land in
// commits 5–7.
//
// For now:
//   sparkplug_b_start() — runs the offline NBIRTH encoding and logs it
//                         (proves the codegen + payload API work).
//   sparkplug_b_stop()  — flips a static flag and logs NDEATH intent.

#include "sparkplug_b.h"
#include "sparkplug_b_payload.h"

#include <stdint.h>

#include "esp_log.h"
#include "esp_timer.h"

#define TAG "sparkplug_b"

// Static state — survives until stop() is called. The MQTT client and
// real bdSeq/seq persistence arrive in commit 5.
static bool s_running = false;
static uint64_t s_bd_seq = 0;

esp_err_t sparkplug_b_start(void)
{
    if (s_running) {
        ESP_LOGW(TAG, "start() called while already running; ignoring");
        return ESP_OK;
    }

    // Build a minimal NBIRTH payload: one BD_Seq metric (the gateway's
    // own rebirth counter) and one Inputs/UptimeMs metric. Real NBIRTHs
    // include the full static template (alias map) — that lands in
    // commit 5 once the sparkplug_b.c state machine knows about the
    // device model.
    sparkplug_metric_t metrics[2] = {0};
    metrics[0].alias = SPARKPLUG_ALIAS_RESERVED;          // placeholder
    metrics[0].name  = "bdSeq";
    metrics[0].data_type = SPARKPLUG_DATA_TYPE_INT64;
    metrics[0].value.int64_value = (int64_t)s_bd_seq;

    metrics[1].alias = SPARKPLUG_ALIAS_UPTIME_MS;
    metrics[1].name  = "Inputs/UptimeMs";
    metrics[1].data_type = SPARKPLUG_DATA_TYPE_INT64;
    metrics[1].value.int64_value = (int64_t)(esp_timer_get_time() / 1000);

    sparkplug_payload_t payload = {
        .timestamp_ms = (uint64_t)(esp_timer_get_time() / 1000),
        .seq = 0U,
        .metrics = metrics,
        .metric_count = 2,
    };

    uint8_t buf[SPARKPLUG_B_PAYLOAD_MAX_BYTES] = {0};
    size_t encoded_len = 0;
    esp_err_t err = sparkplug_b_encode_payload(&payload, buf, sizeof(buf), &encoded_len);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "NBIRTH encode failed: %s", esp_err_to_name(err));
        return err;
    }
    ESP_LOGI(TAG, "NBIRTH encoded (%zu B); bdSeq=%llu (offline — MQTT wiring lands in commit 5)",
             encoded_len, (unsigned long long)s_bd_seq);

    s_running = true;
    return ESP_OK;
}

esp_err_t sparkplug_b_stop(void)
{
    if (!s_running) {
        return ESP_OK;
    }
    s_running = false;
    s_bd_seq = (s_bd_seq + 1) & INT64_MAX;  // bump so next start sends a new BD_Seq
    ESP_LOGI(TAG, "NDEATH intent logged; bdSeq would advance to %llu (offline — MQTT wiring lands in commit 5)",
             (unsigned long long)s_bd_seq);
    return ESP_OK;
}