#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    const char *uri;
    bool tls;
    const char *ca_pem;
    size_t ca_pem_len;
    const char *client_cert_pem;
    size_t client_cert_pem_len;
    const char *client_key_pem;
    size_t client_key_pem_len;
    const char *group_id;
    const char *edge_node_id;
    uint16_t keepalive_sec;
    uint16_t ddeath_timeout_sec;
    uint32_t bd_seq;
} sparkplug_b_config_t;

/** Configure an in-memory client configuration, primarily for tests. */
esp_err_t sparkplug_b_set_config(const sparkplug_b_config_t *cfg);

/** Encode the node birth payload with BD_Seq and Inputs/UptimeMs. */
esp_err_t sparkplug_b_encode_nbirth(uint8_t *out_buf, size_t out_buf_len,
                                    uint64_t bd_seq, uint64_t uptime_ms,
                                    size_t *out_len);

/**
 * @brief Start the Sparkplug B EoN-Node MQTT client.
 *
 * Returns ESP_OK when the MQTT client has started. Birth publication occurs
 * asynchronously after the broker connection event.
 */
esp_err_t sparkplug_b_start(void);

/**
 * @brief Stop the Sparkplug B EoN-Node.
 *
 * Disconnects MQTT (which triggers NDEATH via LWT), tears down the state
 * machine and frees resources. Idempotent.
 */
esp_err_t sparkplug_b_stop(void);

#ifdef __cplusplus
}
#endif
