#pragma once

#include "esp_err.h"

/**
 * @brief Start the MQTT bridge contract.
 *
 * Interface only — full implementation owned by the Fase 4 runtime.
 *
 * @return ESP_OK for the contract-only placeholder.
 */
esp_err_t mqtt_bridge_start(void);

/**
 * @brief Stop the MQTT bridge contract.
 *
 * Interface only — full implementation owned by the Fase 4 runtime.
 *
 * @return ESP_OK for the contract-only placeholder.
 */
esp_err_t mqtt_bridge_stop(void);
