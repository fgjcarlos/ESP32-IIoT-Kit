#include "sparkplug_b.h"

#include <stdio.h>
#include <string.h>

#include "esp_crt_bundle.h"
#include "esp_event.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "mqtt_client.h"
#include "nvs.h"

#include "sparkplug_b_payload.h"
#include "sparkplug_b_state.h"
#include "sparkplug_b_topics.h"

#define TAG "sparkplug_b_mqtt"
#define MQTT_TEXT_MAX 256
#define TLS_FILE_MAX 8192

static esp_mqtt_client_handle_t s_client;
static sparkplug_b_config_t s_config;
static bool s_config_injected;
static bool s_disconnected;
static bool s_ca_bundle_set;
static char s_uri[MQTT_TEXT_MAX];
static char s_group_id[MQTT_TEXT_MAX];
static char s_edge_node_id[MQTT_TEXT_MAX];
static char s_username[MQTT_TEXT_MAX];
static char s_password[MQTT_TEXT_MAX];
static char s_ca_pem[TLS_FILE_MAX];
static char s_cert_pem[TLS_FILE_MAX];
static char s_key_pem[TLS_FILE_MAX];

static esp_err_t nvs_string(nvs_handle_t handle, const char *key, char *value, size_t size,
                            const char *fallback)
{
    size_t length = size;
    esp_err_t err = nvs_get_str(handle, key, value, &length);
    if (err == ESP_ERR_NVS_NOT_FOUND) {
        snprintf(value, size, "%s", fallback);
        return ESP_OK;
    }
    if (err == ESP_OK && length > size) {
        return ESP_ERR_INVALID_SIZE;
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

static esp_err_t load_config_from_nvs(void)
{
    nvs_handle_t handle;
    esp_err_t err = nvs_open("mqtt", NVS_READONLY, &handle);
    if (err != ESP_OK) {
        return err;
    }

    memset(&s_config, 0, sizeof(s_config));
    err = nvs_string(handle, "mqtt_uri", s_uri, sizeof(s_uri), "");
    if (err == ESP_OK) err = nvs_string(handle, "mqtt_group_id", s_group_id, sizeof(s_group_id), "iiot-kit");
    if (err == ESP_OK) err = nvs_string(handle, "mqtt_edge_node_id", s_edge_node_id, sizeof(s_edge_node_id), "GW-UNKNOWN");
    if (err == ESP_OK) err = nvs_string(handle, "mqtt_user", s_username, sizeof(s_username), "");
    if (err == ESP_OK) err = nvs_string(handle, "mqtt_password", s_password, sizeof(s_password), "");
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
    if (s_config.tls) {
        size_t ca_len = 0, cert_len = 0, key_len = 0;
        err = read_file("/spiffs/mqtt/ca.pem", s_ca_pem, sizeof(s_ca_pem), &ca_len);
        if (err == ESP_OK) err = read_file("/spiffs/mqtt/cert.pem", s_cert_pem, sizeof(s_cert_pem), &cert_len);
        if (err == ESP_OK) err = read_file("/spiffs/mqtt/key.pem", s_key_pem, sizeof(s_key_pem), &key_len);
        if (err != ESP_OK) return err;
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
    return ESP_OK;
}

esp_err_t sparkplug_b_encode_nbirth(uint8_t *out_buf, size_t out_buf_len,
                                    uint64_t bd_seq, uint64_t uptime_ms,
                                    size_t *out_len)
{
    sparkplug_metric_t metrics[2] = {
        { .alias = 0, .name = "bdSeq", .data_type = SPARKPLUG_DATA_TYPE_INT64,
          .value.int64_value = (int64_t)bd_seq },
        { .alias = SPARKPLUG_ALIAS_UPTIME_MS, .name = "Inputs/UptimeMs",
          .data_type = SPARKPLUG_DATA_TYPE_INT64, .value.int64_value = (int64_t)uptime_ms },
    };
    sparkplug_payload_t payload = {
        .timestamp_ms = uptime_ms,
        .seq = sparkplug_b_seq_next(),
        .metrics = metrics,
        .metric_count = 2,
    };
    return sparkplug_b_encode_payload(&payload, out_buf, out_buf_len, out_len);
}

static void mqtt_event_handler(void *handler_args, esp_event_base_t base, int32_t event_id,
                               void *event_data)
{
    (void)handler_args;
    (void)base;
    (void)event_data;
    if (event_id == MQTT_EVENT_CONNECTED) {
        if (s_disconnected) {
            s_config.bd_seq++;
            s_disconnected = false;
        }
        char topic[MQTT_TEXT_MAX * 3];
        if (sparkplug_b_topic_nbirth(s_config.group_id, s_config.edge_node_id,
                                     topic, sizeof(topic)) != ESP_OK) {
            ESP_LOGE(TAG, "Failed to build NBIRTH topic");
            return;
        }
        uint8_t payload[SPARKPLUG_B_PAYLOAD_MAX_BYTES];
        size_t payload_len = 0;
        uint64_t bd_seq = s_config.bd_seq;
        if (sparkplug_b_encode_nbirth(payload, sizeof(payload), bd_seq,
                                      (uint64_t)(esp_timer_get_time() / 1000),
                                      &payload_len) != ESP_OK) {
            ESP_LOGE(TAG, "Failed to encode NBIRTH");
            return;
        }
        int message_id = esp_mqtt_client_publish(s_client, topic, (const char *)payload,
                                                 (int)payload_len, 0, 0);
        if (message_id < 0) {
            ESP_LOGE(TAG, "NBIRTH publish failed");
            return;
        }
        nvs_handle_t handle;
        if (nvs_open("mqtt", NVS_READWRITE, &handle) == ESP_OK) {
            esp_err_t err = sparkplug_b_bd_seq_save(handle, s_config.bd_seq);
            nvs_close(handle);
            if (err != ESP_OK) ESP_LOGE(TAG, "Persisting bdSeq failed: %s", esp_err_to_name(err));
        }
        const char *suffixes[] = {"NCMD", "DCMD"};
        for (size_t i = 0; i < 2; ++i) {
            int written = snprintf(topic, sizeof(topic), "spBv1.0/%s/%s/%s%s",
                                   s_config.group_id, suffixes[i], s_config.edge_node_id,
                                   i == 0 ? "" : "/+");
            if (written < 0 || (size_t)written >= sizeof(topic)) continue;
            int sub_id = esp_mqtt_client_subscribe(s_client, topic, 0);
            ESP_LOGI(TAG, "Subscribed to %s (id=%d)", topic, sub_id);
        }
        ESP_LOGI(TAG, "Published NBIRTH to %s", topic);
    } else if (event_id == MQTT_EVENT_DISCONNECTED) {
        s_disconnected = true;
        ESP_LOGW(TAG, "MQTT disconnected");
    }
}

esp_err_t sparkplug_b_mqtt_start(void)
{
    if (s_client != NULL) return ESP_OK;
    if (!s_config_injected) {
        esp_err_t err = load_config_from_nvs();
        if (err != ESP_OK) return err;
    }
    char lwt_topic[MQTT_TEXT_MAX * 3];
    esp_err_t err = sparkplug_b_topic_ndeath(s_config.group_id, s_config.edge_node_id,
                                             lwt_topic, sizeof(lwt_topic));
    if (err != ESP_OK) return err;

    esp_mqtt_client_config_t config = {
        .broker.address.uri = s_config.uri,
        .session.keepalive = s_config.keepalive_sec == 0 ? 60 : s_config.keepalive_sec,
        .session.last_will.topic = lwt_topic,
        .session.last_will.msg = "",
        .session.last_will.msg_len = 0,
        .session.last_will.qos = 0,
        .session.last_will.retain = 0,
        .credentials.username = s_username[0] == '\0' ? NULL : s_username,
        .credentials.authentication.password = s_password[0] == '\0' ? NULL : s_password,
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
    if (s_client == NULL) return ESP_FAIL;
    err = esp_mqtt_client_register_event(s_client, ESP_EVENT_ANY_ID, mqtt_event_handler, NULL);
    if (err != ESP_OK) return err;
    return esp_mqtt_client_start(s_client);
}

esp_err_t sparkplug_b_mqtt_stop(void)
{
    if (s_client == NULL) return ESP_OK;
    esp_err_t err = esp_mqtt_client_stop(s_client);
    esp_mqtt_client_destroy(s_client);
    s_client = NULL;
    return err;
}
