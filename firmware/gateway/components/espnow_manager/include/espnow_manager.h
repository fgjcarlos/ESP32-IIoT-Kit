#pragma once

#include "esp_err.h"

#define ESPNOW_MANAGER_MAX_PEERS 20U

/**
 * @brief Initialize the ESP-NOW manager contract.
 *
 * Interface only — full implementation owned by the Fase 1 T1.4.x change.
 *
 * @return ESP_OK for the contract-only placeholder.
 */
esp_err_t espnow_manager_init(void);
