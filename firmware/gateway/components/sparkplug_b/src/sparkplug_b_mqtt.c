#include "sparkplug_b.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "esp_crt_bundle.h"
#include "esp_event.h"
#include "esp_log.h"
#include "esp_mac.h"
#include "esp_system.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/event_groups.h"
#include "freertos/task.h"
#include "mqtt_client.h"
#include "nvs.h"

#include "protocol.h"
#include "sparkplug_b_cmd.h"
#include "sparkplug_b_device.h"
#include "sparkplug_b_espnow_bridge.h"
#include "sparkplug_b_payload.h"
#include "sparkplug_b_state.h"
#include "sparkplug_b_topics.h"

#define TAG "sparkplug_b_mqtt"
#define MQTT_TEXT_MAX 256
#define TLS_FILE_MAX 8192
#define MQTT_TASK_STACK 6144
#define MQTT_TASK_PRIO 5
#define RECONNECT_BACKOFF_MAX_MS 30000
#define DEVICE_CHECK_PERIOD_MS 1000
// Node level data is republished every 60 s. Sparkplug B v3.0.0 prefers
// Report by Exception, so this is deliberately slow.
#define NDATA_PERIOD_TICKS 60
#define NVS_NAMESPACE "mqtt"

#define EVT_CONNECTED    BIT0
#define EVT_DISCONNECTED BIT1
#define EVT_STOP         BIT2

static esp_mqtt_client_handle_t s_client;
static sparkplug_b_config_t s_config;
static bool s_config_injected;
static bool s_ca_bundle_set;
static bool s_running;
static char s_uri[MQTT_TEXT_MAX];
static char s_group_id[MQTT_TEXT_MAX];
static char s_edge_node_id[MQTT_TEXT_MAX];
static char s_username[MQTT_TEXT_MAX];
static char s_password[MQTT_TEXT_MAX];
// The TLS material is only needed when the broker connection uses TLS. The
// buffers are allocated on demand so a plaintext deployment does not pay
// 3 x TLS_FILE_MAX bytes of static DRAM.
static char *s_ca_pem;
static char *s_cert_pem;
static char *s_key_pem;

// bdSeq of the session that is currently open. The value is persisted as soon
// as the session is established so a reboot continues the sequence instead of
// replaying a bdSeq a Host Application has already seen.
static uint64_t s_session_bd_seq;

// The session task owns the MQTT client: a Sparkplug session needs a fresh
// CONNECT packet (and therefore a fresh bdSeq in the Will) every time the
// connection is re-established, and the ESP-IDF client only accepts the Will
// at creation time.
static EventGroupHandle_t s_events;
static TaskHandle_t s_session_task;

static sparkplug_b_espnow_sink_t s_espnow_sink;
static void *s_espnow_sink_ctx;

static uint64_t now_ms(void)
{
    return (uint64_t)(esp_timer_get_time() / 1000);
}

// Read a string from NVS, falling back to a default when the key is absent.
//
// `found` reports whether the key existed. It cannot be derived from the
// error code alone: nvs_get_str() rejects a NULL length with
// ESP_ERR_NVS_INVALID_LENGTH, which is not the same as "key not found".
static esp_err_t nvs_string(nvs_handle_t handle, const char *key, char *value, size_t size,
                            const char *fallback, bool *found)
{
    if (found != NULL) {
        *found = false;
    }
    size_t length = size;
    esp_err_t err = nvs_get_str(handle, key, value, &length);
    if (err == ESP_ERR_NVS_NOT_FOUND) {
        snprintf(value, size, "%s", fallback);
        return ESP_OK;
    }
    if (err == ESP_OK) {
        if (length > size) {
            return ESP_ERR_INVALID_SIZE;
        }
        if (found != NULL) {
            *found = true;
        }
    }
    return err;
}

static esp_err_t read_file(const char *path, char *buffer, size_t capacity, size_t *length)
{
    FILE *file = fopen(path, "rb");
    if (file == NULL) {
        return ESP_FAIL;
    }
    size_t count = fread(buffer, 1, capacity - 1, file);
    bool failed = ferror(file) != 0;
    int extra = fgetc(file);
    fclose(file);
    if (failed || extra != EOF) {
        return ESP_ERR_INVALID_SIZE;
    }
    buffer[count] = '\0';
    *length = count;
    return count == 0 ? ESP_ERR_INVALID_SIZE : ESP_OK;
}

// Derive a stable identity from the station MAC so the gateway has a
// deterministic edge_node_id even before anyone configures one.
static void derive_edge_node_id(char *out, size_t out_size)
{
    uint8_t mac[6] = {0};
    if (esp_read_mac(mac, ESP_MAC_WIFI_STA) != ESP_OK) {
        snprintf(out, out_size, "GW-UNKNOWN");
        return;
    }
    snprintf(out, out_size, "GW-%02X%02X", mac[4], mac[5]);
}

// Resolve group_id and edge_node_id, persisting them the first time they are
// derived so a later firmware update keeps the same Sparkplug identity.
static esp_err_t resolve_identities(nvs_handle_t handle)
{
    bool dirty = false;
    bool found = false;
    esp_err_t err;

    err = nvs_string(handle, "mqtt_group_id", s_group_id, sizeof(s_group_id), "iiot-kit", &found);
    if (err != ESP_OK) {
        return err;
    }
    if (!found) {
        dirty = true;
    }

    err = nvs_string(handle, "mqtt_edge_node_id", s_edge_node_id, sizeof(s_edge_node_id), "", &found);
    if (err != ESP_OK) {
        return err;
    }
    if (!found || s_edge_node_id[0] == '\0') {
        derive_edge_node_id(s_edge_node_id, sizeof(s_edge_node_id));
        dirty = true;
    }

    if (dirty) {
        err = nvs_set_str(handle, "mqtt_group_id", s_group_id);
        if (err == ESP_OK) {
            err = nvs_set_str(handle, "mqtt_edge_node_id", s_edge_node_id);
        }
        if (err == ESP_OK) {
            err = nvs_commit(handle);
        }
        if (err != ESP_OK) {
            return err;
        }
        ESP_LOGI(TAG, "Persisted Sparkplug identity %s / %s", s_group_id, s_edge_node_id);
    }
    return ESP_OK;
}

static void free_tls_buffers(void);

static esp_err_t allocate_tls_buffers(void)
{
    s_ca_pem = malloc(TLS_FILE_MAX);
    s_cert_pem = malloc(TLS_FILE_MAX);
    s_key_pem = malloc(TLS_FILE_MAX);
    if (s_ca_pem == NULL || s_cert_pem == NULL || s_key_pem == NULL) {
        free_tls_buffers();
        return ESP_ERR_NO_MEM;
    }
    return ESP_OK;
}

static void free_tls_buffers(void)
{
    free(s_ca_pem);
    free(s_cert_pem);
    free(s_key_pem);
    s_ca_pem = NULL;
    s_cert_pem = NULL;
    s_key_pem = NULL;
}

static esp_err_t load_config_from_nvs(void)
{
    nvs_handle_t handle;
    esp_err_t err = nvs_open(NVS_NAMESPACE, NVS_READWRITE, &handle);
    if (err != ESP_OK) {
        return err;
    }

    memset(&s_config, 0, sizeof(s_config));
    err = nvs_string(handle, "mqtt_uri", s_uri, sizeof(s_uri), "", NULL);
    if (err == ESP_OK) err = resolve_identities(handle);
    if (err == ESP_OK) err = nvs_string(handle, "mqtt_user", s_username, sizeof(s_username), "", NULL);
    if (err == ESP_OK) err = nvs_string(handle, "mqtt_password", s_password, sizeof(s_password), "", NULL);
    uint8_t tls = 0;
    uint16_t keepalive = 60;
    uint16_t ddeath_timeout = 1800;
    if (err == ESP_OK) {
        esp_err_t key_err = nvs_get_u8(handle, "mqtt_tls", &tls);
        if (key_err != ESP_OK && key_err != ESP_ERR_NVS_NOT_FOUND) err = key_err;
    }
    if (err == ESP_OK) {
        esp_err_t key_err = nvs_get_u16(handle, "mqtt_keepalive_sec", &keepalive);
        if (key_err != ESP_OK && key_err != ESP_ERR_NVS_NOT_FOUND) err = key_err;
    }
    if (err == ESP_OK) {
        esp_err_t key_err = nvs_get_u16(handle, "mqtt_ddeath_timeout_sec", &ddeath_timeout);
        if (key_err != ESP_OK && key_err != ESP_ERR_NVS_NOT_FOUND) err = key_err;
    }
    uint64_t bd_seq = 0;
    if (err == ESP_OK) {
        esp_err_t key_err = sparkplug_b_bd_seq_load(handle, &bd_seq);
        if (key_err != ESP_OK && key_err != ESP_ERR_NVS_NOT_FOUND) err = key_err;
    }
    nvs_close(handle);
    if (err != ESP_OK) return err;
    if (s_uri[0] == '\0') return ESP_ERR_INVALID_STATE;

    s_config.uri = s_uri;
    s_config.group_id = s_group_id;
    s_config.edge_node_id = s_edge_node_id;
    s_config.tls = tls != 0;
    s_config.keepalive_sec = keepalive;
    s_config.ddeath_timeout_sec = ddeath_timeout;
    s_config.bd_seq = (uint32_t)bd_seq;
    s_session_bd_seq = bd_seq;
    if (s_config.tls) {
        size_t ca_len = 0, cert_len = 0, key_len = 0;
        err = allocate_tls_buffers();
        if (err != ESP_OK) return err;
        err = read_file("/spiffs/mqtt/ca.pem", s_ca_pem, TLS_FILE_MAX, &ca_len);
        if (err == ESP_OK) err = read_file("/spiffs/mqtt/cert.pem", s_cert_pem, TLS_FILE_MAX, &cert_len);
        if (err == ESP_OK) err = read_file("/spiffs/mqtt/key.pem", s_key_pem, TLS_FILE_MAX, &key_len);
        if (err != ESP_OK) {
            free_tls_buffers();
            return err;
        }
        s_config.ca_pem = s_ca_pem;
        s_config.ca_pem_len = ca_len;
        s_config.client_cert_pem = s_cert_pem;
        s_config.client_cert_pem_len = cert_len;
        s_config.client_key_pem = s_key_pem;
        s_config.client_key_pem_len = key_len;
    }
    return ESP_OK;
}

esp_err_t sparkplug_b_set_config(const sparkplug_b_config_t *cfg)
{
    if (cfg == NULL || cfg->uri == NULL || cfg->group_id == NULL || cfg->edge_node_id == NULL) {
        return ESP_ERR_INVALID_ARG;
    }
    s_config = *cfg;
    s_config_injected = true;
    s_session_bd_seq = cfg->bd_seq;
    // The injected configuration owns its strings, so keep local copies: the
    // caller is free to release them once this call returns.
    snprintf(s_uri, sizeof(s_uri), "%s", cfg->uri);
    snprintf(s_group_id, sizeof(s_group_id), "%s", cfg->group_id);
    snprintf(s_edge_node_id, sizeof(s_edge_node_id), "%s", cfg->edge_node_id);
    s_config.uri = s_uri;
    s_config.group_id = s_group_id;
    s_config.edge_node_id = s_edge_node_id;
    return ESP_OK;
}

esp_err_t sparkplug_b_set_espnow_sink(sparkplug_b_espnow_sink_t sink, void *ctx)
{
    s_espnow_sink = sink;
    s_espnow_sink_ctx = ctx;
    return ESP_OK;
}

// ---------------------------------------------------------------------------
// Publishing
// ---------------------------------------------------------------------------

static esp_err_t publish_raw(const char *topic, const uint8_t *payload, size_t len,
                              int qos, int retain)
{
    if (s_client == NULL) {
        return ESP_ERR_INVALID_STATE;
    }
    int message_id = esp_mqtt_client_publish(s_client, topic, (const char *)payload,
                                             (int)len, qos, retain);
    if (message_id < 0) {
        return ESP_FAIL;
    }
    return ESP_OK;
}

// Publish the DBIRTH of a device from a metric set. Defined below, next to
// the rest of the publishing helpers.
static esp_err_t publish_device_birth(const char *device_id,
                                      const sparkplug_metric_t *metrics,
                                      size_t metric_count,
                                      uint64_t timestamp_ms);

// Publish the NBIRTH and the DBIRTH of every known device.
//
// Sparkplug B v3.0.0 §5.5: on a rebirth request the Edge Node MUST send a
// complete birth sequence. The bdSeq is NOT bumped for a rebirth, because no
// new MQTT session is established.
static esp_err_t publish_birth_sequence(void)
{
    char topic[MQTT_TEXT_MAX * 3];
    uint8_t payload[SPARKPLUG_B_PAYLOAD_MAX_BYTES];
    size_t payload_len = 0;

    if (sparkplug_b_topic_nbirth(s_config.group_id, s_config.edge_node_id,
                                 topic, sizeof(topic)) != ESP_OK) {
        return ESP_ERR_INVALID_SIZE;
    }
    if (sparkplug_b_encode_nbirth(payload, sizeof(payload), s_session_bd_seq,
                                  now_ms(), &payload_len) != ESP_OK) {
        return ESP_FAIL;
    }
    esp_err_t err = publish_raw(topic, payload, payload_len, 0, 0);
    if (err != ESP_OK) {
        return err;
    }
    ESP_LOGI(TAG, "Published NBIRTH (bdSeq=%llu, seq=0)",
             (unsigned long long)s_session_bd_seq);

    size_t count = sparkplug_b_devices_count();
    for (size_t i = 0; i < count; ++i) {
        sparkplug_b_device_t device;
        if (sparkplug_b_devices_get(i, &device) != ESP_OK) {
            continue;
        }
        if (device.metric_count == 0) {
            continue;
        }
        // The DBIRTH is rebuilt from the stored metric set so it carries a
        // fresh sequence number.
        err = publish_device_birth(device.device_id, device.metrics,
                                   device.metric_count, now_ms());
        if (err == ESP_OK) {
            ESP_LOGI(TAG, "Published DBIRTH for %s (rebirth)", device.device_id);
        }
    }
    return ESP_OK;
}

// Publish the node level metrics of the EoN-Node.
//
// Sparkplug B v3.0.0 prefers Report by Exception, so NDATA only carries the
// node uptime and is republished on a slow timer rather than on every
// sensor update.
esp_err_t sparkplug_b_publish_ndata(void)
{
    char topic[MQTT_TEXT_MAX * 3];
    if (sparkplug_b_topic_ndata(s_config.group_id, s_config.edge_node_id,
                                topic, sizeof(topic)) != ESP_OK) {
        return ESP_ERR_INVALID_SIZE;
    }

    uint64_t uptime_ms = now_ms();
    sparkplug_metric_t metric = {
        .alias = SPARKPLUG_ALIAS_UPTIME_MS,
        // Sparkplug B v3.0.0 (tck-id-payloads-alias-data-cmd-requirement): a
        // DATA message carries only the alias. The name of alias 11 was
        // established in the NBIRTH, so the Host can resolve it.
        .name = NULL,
        .data_type = SPARKPLUG_DATA_TYPE_INT64,
        .value.int64_value = (int64_t)uptime_ms,
    };
    sparkplug_payload_t payload = {
        .timestamp_ms = uptime_ms,
        .seq = sparkplug_b_seq_next(),
        .metrics = &metric,
        .metric_count = 1,
    };

    uint8_t encoded[SPARKPLUG_B_PAYLOAD_MAX_BYTES];
    size_t encoded_len = 0;
    if (sparkplug_b_encode_payload(&payload, encoded, sizeof(encoded), &encoded_len) != ESP_OK) {
        return ESP_FAIL;
    }
    return publish_raw(topic, encoded, encoded_len, 0, 0);
}

esp_err_t sparkplug_b_publish_ddata(const char *device_id,
                                    const sparkplug_b_ddata_t *data)
{
    if (device_id == NULL || data == NULL) {
        return ESP_ERR_INVALID_ARG;
    }
    char topic[MQTT_TEXT_MAX * 3];
    if (sparkplug_b_topic_ddata(s_config.group_id, s_config.edge_node_id,
                                device_id, topic, sizeof(topic)) != ESP_OK) {
        return ESP_ERR_INVALID_SIZE;
    }

    sparkplug_payload_t payload = {
        .timestamp_ms = data->timestamp_ms,
        .seq = sparkplug_b_seq_next(),
        .metrics = data->metrics,
        .metric_count = data->metric_count,
    };
    uint8_t encoded[SPARKPLUG_B_PAYLOAD_MAX_BYTES];
    size_t encoded_len = 0;
    if (sparkplug_b_encode_payload(&payload, encoded, sizeof(encoded), &encoded_len) != ESP_OK) {
        return ESP_FAIL;
    }

    // A DDATA is never stored: only the metric set of the DBIRTH is kept for
    // the rebirth replay, and a data message must not be republished as a
    // birth.
    esp_err_t err = publish_raw(topic, encoded, encoded_len, 0, 0);
    if (err == ESP_OK) {
        ESP_LOGD(TAG, "Published DDATA for %s (%zu metrics)", device_id, data->metric_count);
    }
    return err;
}

// Publish the DBIRTH of a device from a metric set.
//
// The sequence number always comes from the session counter: Sparkplug
// B v3.0.0 requires every message published after an NBIRTH to carry the
// previous sequence number plus one (tck-id-payloads-dbirth-seq-inc), so a
// rebirth cannot replay the frozen bytes of the previous DBIRTH.
static esp_err_t publish_device_birth(const char *device_id,
                                      const sparkplug_metric_t *metrics,
                                      size_t metric_count,
                                      uint64_t timestamp_ms)
{
    char topic[MQTT_TEXT_MAX * 3];
    if (sparkplug_b_topic_dbirth(s_config.group_id, s_config.edge_node_id,
                                 device_id, topic, sizeof(topic)) != ESP_OK) {
        return ESP_ERR_INVALID_SIZE;
    }

    sparkplug_payload_t payload = {
        .timestamp_ms = timestamp_ms,
        .seq = sparkplug_b_seq_next(),
        .metrics = metrics,
        .metric_count = metric_count,
    };
    uint8_t encoded[SPARKPLUG_B_PAYLOAD_MAX_BYTES];
    size_t encoded_len = 0;
    if (sparkplug_b_encode_payload(&payload, encoded, sizeof(encoded), &encoded_len) != ESP_OK) {
        return ESP_FAIL;
    }
    return publish_raw(topic, encoded, encoded_len, 0, 0);
}

esp_err_t sparkplug_b_publish_dbirth(const char *device_id,
                                     const sparkplug_b_ddata_t *birth)
{
    if (device_id == NULL || birth == NULL) {
        return ESP_ERR_INVALID_ARG;
    }

    esp_err_t err = publish_device_birth(device_id, birth->metrics,
                                         birth->metric_count, birth->timestamp_ms);
    if (err != ESP_OK) {
        return err;
    }

    // Keep the metric set of the birth so a rebirth can publish the DBIRTH of
    // this device again.
    err = sparkplug_b_devices_store_birth(device_id, birth->metrics,
                                           birth->metric_count);
    if (err != ESP_OK) {
        ESP_LOGW(TAG, "Could not store the birth metrics of %s: %s", device_id,
                 esp_err_to_name(err));
    }

    ESP_LOGI(TAG, "Published DBIRTH for %s", device_id);
    return ESP_OK;
}

esp_err_t sparkplug_b_publish_ddeath(const char *device_id)
{
    if (device_id == NULL) {
        return ESP_ERR_INVALID_ARG;
    }
    char topic[MQTT_TEXT_MAX * 3];
    if (sparkplug_b_topic_ddeath(s_config.group_id, s_config.edge_node_id,
                                 device_id, topic, sizeof(topic)) != ESP_OK) {
        return ESP_ERR_INVALID_SIZE;
    }

    // A DDEATH carries a sequence number and the bdSeq metric of the session
    // the device belonged to.
    sparkplug_metric_t metric = {
        .alias = SPARKPLUG_ALIAS_NONE,
        .name = SPARKPLUG_METRIC_BDSEQ_NAME,
        .data_type = SPARKPLUG_DATA_TYPE_INT64,
        .value.int64_value = (int64_t)s_session_bd_seq,
    };
    sparkplug_payload_t payload = {
        .timestamp_ms = now_ms(),
        .seq = sparkplug_b_seq_next(),
        .metrics = &metric,
        .metric_count = 1,
    };

    uint8_t encoded[SPARKPLUG_B_PAYLOAD_MAX_BYTES];
    size_t encoded_len = 0;
    if (sparkplug_b_encode_payload(&payload, encoded, sizeof(encoded), &encoded_len) != ESP_OK) {
        return ESP_FAIL;
    }
    esp_err_t err = publish_raw(topic, encoded, encoded_len, 0, 0);
    if (err == ESP_OK) {
        ESP_LOGW(TAG, "Published DDEATH for %s", device_id);
    }
    return err;
}

esp_err_t sparkplug_b_on_espnow_data(const msg_sensor_data_t *data)
{
    if (data == NULL) {
        return ESP_ERR_INVALID_ARG;
    }
    sparkplug_b_ddata_t translated;
    esp_err_t err = sparkplug_b_espnow_bridge_on_data(data, &translated);
    if (err != ESP_OK) {
        return err;
    }
    sparkplug_b_devices_touch(translated.device_id, data->header.node_mac, now_ms());
    return sparkplug_b_publish_ddata(translated.device_id, &translated);
}

esp_err_t sparkplug_b_on_espnow_discovery(const msg_discovery_t *disc)
{
    if (disc == NULL) {
        return ESP_ERR_INVALID_ARG;
    }
    sparkplug_b_ddata_t translated;
    esp_err_t err = sparkplug_b_espnow_bridge_on_discovery(disc, &translated);
    if (err != ESP_OK) {
        return err;
    }
    sparkplug_b_devices_touch(translated.device_id, disc->header.node_mac, now_ms());
    return sparkplug_b_publish_dbirth(translated.device_id, &translated);
}

// ---------------------------------------------------------------------------
// Command handling
// ---------------------------------------------------------------------------

// Forward a translated command to the node behind the device.
static void dispatch_to_node(const char *device_id, const sparkplug_b_command_t *cmd)
{
    if (s_espnow_sink == NULL) {
        ESP_LOGW(TAG, "No ESP-NOW sink registered, dropping command %s",
                 sparkplug_b_cmd_kind_str(cmd->kind));
        return;
    }

    size_t index = 0;
    if (sparkplug_b_devices_find(device_id, &index) != ESP_OK) {
        ESP_LOGW(TAG, "Unknown device %s, dropping command %s", device_id,
                 sparkplug_b_cmd_kind_str(cmd->kind));
        return;
    }
    sparkplug_b_device_t device;
    if (sparkplug_b_devices_get(index, &device) != ESP_OK) {
        return;
    }

    uint8_t frame[SPARKPLUG_B_CMD_FRAME_MAX];
    size_t frame_len = 0;
    esp_err_t err = sparkplug_b_cmd_to_espnow(cmd, device.mac,
                                               (uint16_t)sparkplug_b_seq_next(),
                                               frame, sizeof(frame), &frame_len);
    if (err != ESP_OK) {
        ESP_LOGW(TAG, "Command %s cannot be sent over ESP-NOW: %s",
                 sparkplug_b_cmd_kind_str(cmd->kind), esp_err_to_name(err));
        return;
    }
    err = s_espnow_sink(frame, frame_len, s_espnow_sink_ctx);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "ESP-NOW sink rejected the frame: %s", esp_err_to_name(err));
        return;
    }
    ESP_LOGI(TAG, "Forwarded %s to %s as a %zu byte ESP-NOW frame",
             sparkplug_b_cmd_kind_str(cmd->kind), device_id, frame_len);
}

static void handle_command(bool device_command, const char *device_id,
                           const uint8_t *payload, size_t len)
{
    sparkplug_b_decoded_metric_t metrics[SPARKPLUG_B_MAX_DECODED_METRICS];
    size_t metric_count = 0;
    esp_err_t err = sparkplug_b_decode_metrics(payload, len, metrics,
                                               SPARKPLUG_B_MAX_DECODED_METRICS,
                                               &metric_count);
    if (err != ESP_OK) {
        ESP_LOGW(TAG, "Discarding malformed command payload (%zu bytes)", len);
        return;
    }

    sparkplug_b_command_t cmd;
    err = sparkplug_b_cmd_parse(metrics, metric_count, device_command, &cmd);
    if (err != ESP_OK) {
        return;
    }

    switch (cmd.kind) {
    case SPARKPLUG_CMD_REBIRTH:
        ESP_LOGI(TAG, "Rebirth requested");
        // A rebirth does not open a new MQTT session, so the bdSeq stays.
        publish_birth_sequence();
        return;
    case SPARKPLUG_CMD_NODE_REBOOT:
        ESP_LOGW(TAG, "Reboot requested through Node Control/Reboot");
        esp_restart();
        return;
    case SPARKPLUG_CMD_DEVICE_REBIRTH:
        ESP_LOGI(TAG, "Rebirth requested for %s", device_id);
        if (device_id != NULL) {
            size_t index = 0;
            if (sparkplug_b_devices_find(device_id, &index) == ESP_OK) {
                sparkplug_b_device_t device;
                if (sparkplug_b_devices_get(index, &device) == ESP_OK &&
                    device.metric_count > 0) {
                    publish_device_birth(device_id, device.metrics,
                                         device.metric_count, now_ms());
                }
            }
        }
        return;
    case SPARKPLUG_CMD_DEVICE_REBOOT:
    case SPARKPLUG_CMD_SET_SLEEP:
        if (device_id == NULL) {
            ESP_LOGW(TAG, "Command %s needs a device, none given",
                     sparkplug_b_cmd_kind_str(cmd.kind));
            return;
        }
        dispatch_to_node(device_id, &cmd);
        return;
    case SPARKPLUG_CMD_UNSUPPORTED:
        ESP_LOGW(TAG, "Discarding unsupported command");
        return;
    case SPARKPLUG_CMD_NONE:
    default:
        return;
    }
}

// ---------------------------------------------------------------------------
// Session lifecycle
// ---------------------------------------------------------------------------

static void persist_bd_seq(void)
{
    nvs_handle_t handle;
    if (nvs_open(NVS_NAMESPACE, NVS_READWRITE, &handle) != ESP_OK) {
        return;
    }
    esp_err_t err = sparkplug_b_bd_seq_save(handle, s_session_bd_seq);
    nvs_close(handle);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Persisting bdSeq failed: %s", esp_err_to_name(err));
    }
}

static esp_err_t create_client(void)
{
    char lwt_topic[MQTT_TEXT_MAX * 3];
    esp_err_t err = sparkplug_b_topic_ndeath(s_config.group_id, s_config.edge_node_id,
                                             lwt_topic, sizeof(lwt_topic));
    if (err != ESP_OK) {
        return err;
    }

    // The death certificate has to carry the bdSeq of the session it belongs
    // to, and the broker publishes it when the session ends ungracefully.
    uint8_t lwt_payload[SPARKPLUG_B_PAYLOAD_MAX_BYTES];
    size_t lwt_len = 0;
    err = sparkplug_b_encode_death(lwt_payload, sizeof(lwt_payload),
                                   s_session_bd_seq, now_ms(), &lwt_len);
    if (err != ESP_OK) {
        return err;
    }

    esp_mqtt_client_config_t config = {
        .broker.address.uri = s_config.uri,
        .session.keepalive = s_config.keepalive_sec == 0 ? 60 : s_config.keepalive_sec,
        .session.last_will.topic = lwt_topic,
        .session.last_will.msg = (const char *)lwt_payload,
        .session.last_will.msg_len = (int)lwt_len,
        // Sparkplug B v3.0.0 requires the Will to use QoS 1 and to be
        // delivered without the retained flag.
        .session.last_will.qos = 1,
        .session.last_will.retain = 0,
        .credentials.username = s_username[0] == '\0' ? NULL : s_username,
        .credentials.authentication.password = s_password[0] == '\0' ? NULL : s_password,
        // The session task drives the reconnects, because every new CONNECT
        // needs a new bdSeq and therefore a new Will.
        .network.disable_auto_reconnect = true,
    };
    if (s_config.tls) {
        if (s_config.ca_pem == NULL || s_config.client_cert_pem == NULL || s_config.client_key_pem == NULL) {
            return ESP_ERR_INVALID_ARG;
        }
        if (!s_ca_bundle_set) {
            err = esp_crt_bundle_set((const uint8_t *)s_config.ca_pem, s_config.ca_pem_len);
            if (err != ESP_OK) return err;
            s_ca_bundle_set = true;
        }
        config.broker.verification.crt_bundle_attach = esp_crt_bundle_attach;
        config.credentials.authentication.certificate = s_config.client_cert_pem;
        config.credentials.authentication.key = s_config.client_key_pem;
    }
    s_client = esp_mqtt_client_init(&config);
    if (s_client == NULL) {
        return ESP_FAIL;
    }
    return ESP_OK;
}

static void destroy_client(void)
{
    if (s_client == NULL) {
        return;
    }
    esp_mqtt_client_stop(s_client);
    esp_mqtt_client_destroy(s_client);
    s_client = NULL;
}

static void subscribe_commands(void)
{
    char topic[MQTT_TEXT_MAX * 3];
    const char *suffixes[] = {"NCMD", "DCMD"};
    for (size_t i = 0; i < 2; ++i) {
        int written = snprintf(topic, sizeof(topic), "spBv1.0/%s/%s/%s%s",
                               s_config.group_id, suffixes[i], s_config.edge_node_id,
                               i == 0 ? "" : "/+");
        if (written < 0 || (size_t)written >= sizeof(topic)) continue;
        // Sparkplug B v3.0.0 requires the NCMD subscription to use QoS 1.
        int sub_id = esp_mqtt_client_subscribe(s_client, topic, 1);
        ESP_LOGI(TAG, "Subscribed to %s (id=%d)", topic, sub_id);
    }
}

// Publish a DDEATH for every device that stopped reporting.
static void expire_devices(void)
{
    size_t expired[SPARKPLUG_B_MAX_DEVICES];
    size_t count = sparkplug_b_devices_expire(now_ms(), s_config.ddeath_timeout_sec,
                                               expired, SPARKPLUG_B_MAX_DEVICES);
    for (size_t i = 0; i < count && i < SPARKPLUG_B_MAX_DEVICES; ++i) {
        sparkplug_b_device_t device;
        if (sparkplug_b_devices_get(expired[i], &device) != ESP_OK) {
            continue;
        }
        sparkplug_b_publish_ddeath(device.device_id);
    }
}

static void mqtt_event_handler(void *handler_args, esp_event_base_t base, int32_t event_id,
                               void *event_data)
{
    (void)handler_args;
    (void)base;

    if (event_id == MQTT_EVENT_CONNECTED) {
        xEventGroupSetBits(s_events, EVT_CONNECTED);
        ESP_LOGI(TAG, "MQTT connected");
        return;
    }

    if (event_id == MQTT_EVENT_DISCONNECTED) {
        xEventGroupSetBits(s_events, EVT_DISCONNECTED);
        ESP_LOGW(TAG, "MQTT disconnected");
        return;
    }

    if (event_id == MQTT_EVENT_DATA) {
        esp_mqtt_event_handle_t event = (esp_mqtt_event_handle_t)event_data;
        if (event == NULL || event->topic == NULL || event->data == NULL) {
            return;
        }
        // NCMD and DCMD are the only topics this gateway acts on.
        if (strstr(event->topic, "/NCMD/") != NULL) {
            handle_command(false, NULL, (const uint8_t *)event->data, event->data_len);
        } else if (strstr(event->topic, "/DCMD/") != NULL) {
            // The device id is the last segment of the topic.
            const char *device_id = strrchr(event->topic, '/');
            device_id = device_id != NULL ? device_id + 1 : NULL;
            handle_command(true, device_id, (const uint8_t *)event->data, event->data_len);
        }
        return;
    }
}

static void mqtt_session_task(void *arg)
{
    (void)arg;
    uint32_t backoff_ms = 1000;

    while (true) {
        if (s_events != NULL &&
            (xEventGroupGetBits(s_events) & EVT_STOP) != 0) {
            break;
        }

        esp_err_t err = create_client();
        if (err != ESP_OK) {
            ESP_LOGE(TAG, "Client creation failed: %s", esp_err_to_name(err));
            vTaskDelay(pdMS_TO_TICKS(backoff_ms));
            backoff_ms = backoff_ms * 2 > RECONNECT_BACKOFF_MAX_MS
                             ? RECONNECT_BACKOFF_MAX_MS : backoff_ms * 2;
            continue;
        }

        err = esp_mqtt_client_register_event(s_client, ESP_EVENT_ANY_ID, mqtt_event_handler, NULL);
        if (err == ESP_OK) {
            err = esp_mqtt_client_start(s_client);
        }
        if (err != ESP_OK) {
            ESP_LOGE(TAG, "Client start failed: %s", esp_err_to_name(err));
            destroy_client();
            vTaskDelay(pdMS_TO_TICKS(backoff_ms));
            backoff_ms = backoff_ms * 2 > RECONNECT_BACKOFF_MAX_MS
                             ? RECONNECT_BACKOFF_MAX_MS : backoff_ms * 2;
            continue;
        }

        // Wait for the session to come up.
        EventBits_t bits = xEventGroupWaitBits(s_events, EVT_CONNECTED | EVT_STOP,
                                                pdFALSE, pdFALSE, portMAX_DELAY);
        if ((bits & EVT_STOP) != 0) {
            destroy_client();
            break;
        }
        xEventGroupClearBits(s_events, EVT_CONNECTED);
        backoff_ms = 1000;

        // The session is up: publish the birth sequence and start listening
        // for commands.
        publish_birth_sequence();
        subscribe_commands();

        // The next session gets a new bdSeq. Persisting it here means a
        // reboot continues the sequence instead of replaying a value a Host
        // Application has already seen.
        s_session_bd_seq = sparkplug_b_bd_seq_next();
        persist_bd_seq();

        // Stay connected until the session drops. The wait has a timeout so
        // the device expiry policy and the node data report are checked
        // periodically.
        bool stop_requested = false;
        uint32_t ndata_ticks = 0;
        while (!stop_requested) {
            bits = xEventGroupWaitBits(s_events, EVT_DISCONNECTED | EVT_STOP,
                                       pdFALSE, pdFALSE,
                                       pdMS_TO_TICKS(DEVICE_CHECK_PERIOD_MS));
            if ((bits & EVT_STOP) != 0) {
                stop_requested = true;
                break;
            }
            if ((bits & EVT_DISCONNECTED) != 0) {
                break;
            }
            expire_devices();

            // Node level data on a slow timer. Sparkplug prefers Report by
            // Exception, so this only carries the node uptime.
            if (++ndata_ticks >= NDATA_PERIOD_TICKS) {
                ndata_ticks = 0;
                sparkplug_b_publish_ndata();
            }
        }
        xEventGroupClearBits(s_events, EVT_DISCONNECTED);
        destroy_client();
        if (stop_requested) {
            break;
        }

        // Reconnect with a backoff so an unreachable broker is not hammered.
        vTaskDelay(pdMS_TO_TICKS(backoff_ms));
        backoff_ms = backoff_ms * 2 > RECONNECT_BACKOFF_MAX_MS
                         ? RECONNECT_BACKOFF_MAX_MS : backoff_ms * 2;
    }

    destroy_client();
    s_session_task = NULL;
    vTaskDelete(NULL);
}

esp_err_t sparkplug_b_mqtt_start(void)
{
    if (s_running) {
        return ESP_OK;
    }
    if (!s_config_injected) {
        esp_err_t err = load_config_from_nvs();
        if (err != ESP_OK) return err;
    }
    if (s_config.uri == NULL || s_config.uri[0] == '\0') {
        return ESP_ERR_INVALID_STATE;
    }

    esp_err_t err = sparkplug_b_devices_init();
    if (err != ESP_OK) {
        return err;
    }

    s_events = xEventGroupCreate();
    if (s_events == NULL) {
        return ESP_ERR_NO_MEM;
    }
    xEventGroupClearBits(s_events, EVT_CONNECTED | EVT_DISCONNECTED | EVT_STOP);

    s_running = true;
    if (xTaskCreate(mqtt_session_task, "sparkplug_b_mqtt", MQTT_TASK_STACK, NULL,
                    MQTT_TASK_PRIO, &s_session_task) != pdPASS) {
        s_running = false;
        vEventGroupDelete(s_events);
        s_events = NULL;
        return ESP_ERR_NO_MEM;
    }
    ESP_LOGI(TAG, "Sparkplug B session task started (group=%s edge=%s)",
             s_config.group_id, s_config.edge_node_id);
    return ESP_OK;
}

esp_err_t sparkplug_b_mqtt_stop(void)
{
    if (!s_running) {
        return ESP_OK;
    }
    s_running = false;
    if (s_events != NULL) {
        xEventGroupSetBits(s_events, EVT_STOP);
    }
    if (s_session_task != NULL) {
        // The task clears the handle when it exits.
        for (int i = 0; i < 50 && s_session_task != NULL; ++i) {
            vTaskDelay(pdMS_TO_TICKS(20));
        }
    }
    if (s_events != NULL) {
        vEventGroupDelete(s_events);
        s_events = NULL;
    }
    s_session_task = NULL;
    destroy_client();
    free_tls_buffers();
    return ESP_OK;
}
