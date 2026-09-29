#pragma once

#include "esp_err.h"

/**
 * @brief Initialize the OTA manager contract.
 *
 * Interface only — full implementation owned by the Fase 5 T5.x change
 * (OTA partition rotation, rollback, and signed-image verification).
 *
 * @return ESP_OK for the contract-only placeholder.
 */
esp_err_t ota_manager_init(void);