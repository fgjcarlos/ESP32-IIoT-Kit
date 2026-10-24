#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "esp_err.h"

#include "protocol.h"
#include "sparkplug_b_espnow_bridge.h"

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

/**
 * @brief Start the Sparkplug B EoN-Node MQTT client.
 *
 * Returns ESP_OK when the session task has been created. The NBIRTH is
 * published asynchronously once the broker accepts the connection.
 */
esp_err_t sparkplug_b_start(void);

/**
 * @brief Stop the Sparkplug B EoN-Node.
 *
 * Disconnects MQTT (which triggers NDEATH via the Will), tears down the
 * session task and frees resources. Idempotent.
 */
esp_err_t sparkplug_b_stop(void);

/**
 * @brief Sink that receives the ESP-NOW frames produced by a command.
 *
 * The Sparkplug module never talks to the radio directly: it translates a
 * command into an ESP-NOW frame and hands it to the sink registered by the
 * application, which forwards it to the node.
 *
 * @param frame  Serialized ESP-NOW frame.
 * @param len    Frame length in bytes.
 * @param ctx    Opaque context handed back to the sink.
 */
typedef esp_err_t (*sparkplug_b_espnow_sink_t)(const uint8_t *frame, size_t len,
                                               void *ctx);

/** @brief Register the sink that receives translated ESP-NOW frames. */
esp_err_t sparkplug_b_set_espnow_sink(sparkplug_b_espnow_sink_t sink, void *ctx);

/**
 * @brief Translate an ESP-NOW data frame and publish it as a DDATA.
 *
 * Entry point for the ESP-NOW receive path. The frame is translated by the
 * bridge and published on the DDATA topic of the device behind it.
 */
esp_err_t sparkplug_b_on_espnow_data(const msg_sensor_data_t *data);

/**
 * @brief Translate an ESP-NOW discovery frame and publish it as a DBIRTH.
 *
 * Entry point for the ESP-NOW receive path. The frame is translated by the
 * bridge and published on the DBIRTH topic of the device behind it.
 */
esp_err_t sparkplug_b_on_espnow_discovery(const msg_discovery_t *disc);

/**
 * @brief Publish the node level metrics of the EoN-Node as an NDATA.
 *
 * Sparkplug B v3.0.0 prefers Report by Exception, so the NDATA only carries
 * the node uptime and is republished on a slow timer.
 */
esp_err_t sparkplug_b_publish_ndata(void);

/**
 * @brief Publish a DDATA for a device from an already translated payload.
 *
 * Used by the ESP-NOW path once the bridge produced the metrics.
 */
esp_err_t sparkplug_b_publish_ddata(const char *device_id,
                                    const sparkplug_b_ddata_t *data);

/** @brief Publish a DBIRTH for a device from an already translated payload. */
esp_err_t sparkplug_b_publish_dbirth(const char *device_id,
                                     const sparkplug_b_ddata_t *birth);

/** @brief Publish a DDEATH for a device that stopped reporting. */
esp_err_t sparkplug_b_publish_ddeath(const char *device_id);

#ifdef __cplusplus
}
#endif
