#pragma once

#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Start the Sparkplug B EoN-Node.
 *
 * Returns ESP_OK when the MQTT client connects, NBIRTH is published and the
 * state machine is online. Returns an esp_err_t otherwise.
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
