#pragma once

#include "esp_err.h"

/**
 * @brief Start the HTTP server contract.
 *
 * Interface only — full implementation owned by the Fase 4 runtime.
 *
 * @return ESP_OK for the contract-only placeholder.
 */
esp_err_t http_server_start(void);

/**
 * @brief Stop the HTTP server contract.
 *
 * Interface only — full implementation owned by the Fase 4 runtime.
 *
 * @return ESP_OK for the contract-only placeholder.
 */
esp_err_t http_server_stop(void);
