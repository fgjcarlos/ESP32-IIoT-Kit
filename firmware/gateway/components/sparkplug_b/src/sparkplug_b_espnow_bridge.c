// ESP-NOW → Sparkplug B translation bridge (issue #11, commit 6).
//
// Implements the contract declared in include/sparkplug_b_espnow_bridge.h.
// All metric ordering and suppression rules are documented in that header
// and in odd/tasks/issue-11-sparkplug-b/design.md §3.5.

#include "sparkplug_b_espnow_bridge.h"

#include <stddef.h>
#include <string.h>

#include "esp_log.h"
#include "esp_timer.h"

#include "sparkplug_b_state.h"

#define TAG "spkplug_bridge"

// Internal aliases for typed sensors. We keep this table in .c so the header
// stays as a pure contract surface and the Sparkplug metric shape remains
// owned by sparkplug_b_payload.
static const struct {
    sensor_type_t sensor;
    const char *name;
    uint64_t alias;
} k_typed_metrics[] = {
    {SENSOR_TYPE_TEMPERATURE, "Inputs/Temperature",     SPARKPLUG_ALIAS_TEMP},
    {SENSOR_TYPE_PH,          "Inputs/Ph",              SPARKPLUG_ALIAS_PH},
    {SENSOR_TYPE_DO,          "Inputs/Do",              SPARKPLUG_ALIAS_DO},
    {SENSOR_TYPE_ORP,         "Inputs/Orp",             SPARKPLUG_ALIAS_ORP},
    {SENSOR_TYPE_LEVEL,       "Inputs/Level",           SPARKPLUG_ALIAS_LEVEL},
    {SENSOR_TYPE_TURBIDITY,   "Inputs/Turbidity",       SPARKPLUG_ALIAS_TURBIDITY},
};

static const char *const k_custom_names[PROTOCOL_MAX_VALUES] = {
    "Inputs/Custom/0",
    "Inputs/Custom/1",
    "Inputs/Custom/2",
    "Inputs/Custom/3",
};

static char s_group_id[32];
static char s_edge_node_id[32];
static bool s_initialized = false;
static bool s_warned_no_sntp = false;

// ------------------------------ helpers -------------------------------------

static esp_err_t copy_id(const char *src, char *dst, size_t dst_size)
{
    if (src == NULL || dst == NULL || dst_size == 0) {
        return ESP_ERR_INVALID_ARG;
    }
    size_t len = strlen(src);
    if (len == 0 || len >= dst_size) {
        return ESP_ERR_INVALID_ARG;
    }
    memcpy(dst, src, len + 1);
    return ESP_OK;
}

static void append_double(sparkplug_metric_t *m, uint64_t alias, const char *name, double v)
{
    m->alias = alias;
    m->name = (name != NULL) ? name : NULL;
    m->data_type = SPARKPLUG_DATA_TYPE_DOUBLE;
    m->value.double_value = v;
}

static uint64_t monotonic_ms(void)
{
    if (!s_warned_no_sntp) {
        ESP_LOGW(TAG,
                 "using monotonic timestamp (esp_timer_get_time); SNTP wiring lands in Phase 5");
        s_warned_no_sntp = true;
    }
    return (uint64_t)(esp_timer_get_time() / 1000ULL);
}

static esp_err_t device_id_for_mac(const uint8_t mac[6], char *buf, size_t buf_len)
{
    if (mac == NULL) {
        return ESP_ERR_INVALID_ARG;
    }
    // "N-" + 4 hex chars (last MAC byte, uppercase) + NUL = 7 bytes minimum.
    int n = snprintf(buf, buf_len, "N-%02X%02X", mac[4], mac[5]);
    if (n < 0 || (size_t)n >= buf_len) {
        return ESP_ERR_INVALID_SIZE;
    }
    return ESP_OK;
}

// ----------------------------- public API -----------------------------------

esp_err_t sparkplug_b_espnow_bridge_init(const char *group_id, const char *edge_node_id)
{
    if (group_id == NULL || edge_node_id == NULL ||
        group_id[0] == '\0' || edge_node_id[0] == '\0') {
        return ESP_ERR_INVALID_ARG;
    }
    esp_err_t err = copy_id(group_id, s_group_id, sizeof(s_group_id));
    if (err != ESP_OK) {
        return err;
    }
    err = copy_id(edge_node_id, s_edge_node_id, sizeof(s_edge_node_id));
    if (err != ESP_OK) {
        return err;
    }
    s_initialized = true;
    return ESP_OK;
}

esp_err_t sparkplug_b_espnow_bridge_device_id_for(const uint8_t mac[6],
                                                  char *buf, size_t buf_len)
{
    if (buf == NULL || buf_len == 0) {
        return ESP_ERR_INVALID_ARG;
    }
    return device_id_for_mac(mac, buf, buf_len);
}

// Shared emit-typed-metric path. Returns the alias used, or 0 if not a typed
// sensor (CUSTOM, unknown). For unknown sensor types we still emit nothing
// here; caller must decide how to handle the untyped case.
static uint64_t emit_typed_metric(sparkplug_metric_t *slot, sensor_type_t sensor,
                                  float v0)
{
    for (size_t i = 0; i < sizeof(k_typed_metrics) / sizeof(k_typed_metrics[0]); ++i) {
        if (k_typed_metrics[i].sensor == sensor) {
            append_double(slot, k_typed_metrics[i].alias,
                          k_typed_metrics[i].name, (double)v0);
            return k_typed_metrics[i].alias;
        }
    }
    return 0;
}

// Append a Custom slot to out->metrics at the given index. Returns the new
// metric_count, or 0 on overflow (and logs once).
static size_t append_custom(sparkplug_b_ddata_t *out, size_t idx, size_t slot_idx, float v)
{
    if (idx >= sizeof(out->metrics) / sizeof(out->metrics[0])) {
        return idx;
    }
    append_double(&out->metrics[idx], SPARKPLUG_ALIAS_CUSTOM_0 + slot_idx,
                  k_custom_names[slot_idx], (double)v);
    return idx + 1;
}

// Append the standard diagnostics (Uptime/Battery/Rssi). Battery is
// suppressed when the wire format says "unknown" (0xFF); uptime is always
// present (0 is a legal value); rssi is always present.
static size_t append_diagnostics(sparkplug_b_ddata_t *out, size_t idx,
                                 uint32_t uptime_ms, uint8_t battery_pct, int8_t rssi)
{
    if (idx < sizeof(out->metrics) / sizeof(out->metrics[0])) {
        sparkplug_metric_t *m = &out->metrics[idx++];
        m->alias = SPARKPLUG_ALIAS_UPTIME_MS;
        m->name = "Diagnostics/UptimeMs";
        m->data_type = SPARKPLUG_DATA_TYPE_INT64;
        m->value.int64_value = (int64_t)uptime_ms;
    }
    if (battery_pct != 0xFF &&
        idx < sizeof(out->metrics) / sizeof(out->metrics[0])) {
        sparkplug_metric_t *m = &out->metrics[idx++];
        m->alias = SPARKPLUG_ALIAS_BATTERY;
        m->name = "Diagnostics/BatteryPercent";
        m->data_type = SPARKPLUG_DATA_TYPE_FLOAT;
        m->value.float_value = (float)battery_pct;
    }
    if (idx < sizeof(out->metrics) / sizeof(out->metrics[0])) {
        sparkplug_metric_t *m = &out->metrics[idx++];
        m->alias = SPARKPLUG_ALIAS_RSSI;
        m->name = "Diagnostics/RssiLast";
        m->data_type = SPARKPLUG_DATA_TYPE_INT8;
        m->value.int8_value = rssi;
    }
    return idx;
}

esp_err_t sparkplug_b_espnow_bridge_on_data(const msg_sensor_data_t *data,
                                            sparkplug_b_ddata_t *out)
{
    if (data == NULL || out == NULL) {
        return ESP_ERR_INVALID_ARG;
    }
    if (!s_initialized) {
        ESP_LOGE(TAG, "on_data called before init");
        return ESP_ERR_INVALID_STATE;
    }

    memset(out, 0, sizeof(*out));
    esp_err_t err = device_id_for_mac(data->header.node_mac, out->device_id, sizeof(out->device_id));
    if (err != ESP_OK) {
        return err;
    }

    out->timestamp_ms = monotonic_ms();
    out->seq = sparkplug_b_seq_next();

    size_t n = 0;
    if (data->sensor_type == SENSOR_TYPE_CUSTOM) {
        // All slots go to Custom/0..3.
        size_t max_slots = data->value_count;
        if (max_slots > PROTOCOL_MAX_VALUES) {
            max_slots = PROTOCOL_MAX_VALUES;
        }
        for (size_t i = 0; i < max_slots; ++i) {
            n = append_custom(out, n, i, data->values[i]);
        }
    } else {
        // Typed sensor. values[0] is the primary reading; values[1..N-1]
        // become Custom/0..N-2 (capped at PROTOCOL_MAX_VALUES - 1).
        if (data->value_count >= 1) {
            if (emit_typed_metric(&out->metrics[n], data->sensor_type, data->values[0]) != 0) {
                n++;
            } else {
                ESP_LOGW(TAG, "unknown sensor type 0x%02x; dropping primary metric",
                         (unsigned)data->sensor_type);
            }
        }
        size_t extras = 0;
        if (data->value_count > 1) {
            extras = data->value_count - 1;
        }
        if (extras > PROTOCOL_MAX_VALUES - 1) {
            extras = PROTOCOL_MAX_VALUES - 1;
        }
        for (size_t i = 0; i < extras; ++i) {
            n = append_custom(out, n, i, data->values[1 + i]);
        }
    }

    n = append_diagnostics(out, n, data->uptime_ms, data->battery_pct, data->rssi_last);
    out->metric_count = n;
    return ESP_OK;
}

esp_err_t sparkplug_b_espnow_bridge_on_discovery(const msg_discovery_t *disc,
                                                sparkplug_b_ddata_t *out)
{
    if (disc == NULL || out == NULL) {
        return ESP_ERR_INVALID_ARG;
    }
    if (!s_initialized) {
        ESP_LOGE(TAG, "on_discovery called before init");
        return ESP_ERR_INVALID_STATE;
    }

    memset(out, 0, sizeof(*out));
    esp_err_t err = device_id_for_mac(disc->header.node_mac, out->device_id, sizeof(out->device_id));
    if (err != ESP_OK) {
        return err;
    }

    out->timestamp_ms = monotonic_ms();
    out->seq = sparkplug_b_seq_next();

    size_t n = 0;

    // DBIRTH primary metric mirrors the sensor's typed value. For unknown
    // sensor types (CUSTOM, unrecognised), we still emit Custom/0 so the
    // Host has at least one stable alias to bind to.
    if (disc->sensor_type == SENSOR_TYPE_CUSTOM) {
        n = append_custom(out, n, 0, 0.0);
    } else {
        if (emit_typed_metric(&out->metrics[n], disc->sensor_type, 0.0f) != 0) {
            n++;
        } else {
            n = append_custom(out, n, 0, 0.0);
        }
    }

    // Diagnostics: battery (if known), uptime=0 placeholder is implicit.
    if (disc->battery_pct != 0xFF) {
        if (n < sizeof(out->metrics) / sizeof(out->metrics[0])) {
            sparkplug_metric_t *m = &out->metrics[n++];
            m->alias = SPARKPLUG_ALIAS_BATTERY;
            m->name = "Diagnostics/BatteryPercent";
            m->data_type = SPARKPLUG_DATA_TYPE_FLOAT;
            m->value.float_value = (float)disc->battery_pct;
        }
    }
    // Sleep interval — keep it as a Custom/1 placeholder until commit 9
    // adds a dedicated Properties/SleepIntervalSec alias. Out of commit 6
    // scope but documented in design.md §3.5.
    if (n < sizeof(out->metrics) / sizeof(out->metrics[0])) {
        sparkplug_metric_t *m = &out->metrics[n++];
        m->alias = SPARKPLUG_ALIAS_CUSTOM_0 + 1;
        m->name = "Inputs/Custom/1";
        m->data_type = SPARKPLUG_DATA_TYPE_DOUBLE;
        m->value.double_value = (double)disc->sleep_interval_sec;
    }

    out->metric_count = n;
    return ESP_OK;
}