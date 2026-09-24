#pragma once

#include <stddef.h>
#include <stdint.h>

#include "esp_err.h"
#include "esp_chip_info.h"

typedef struct {
    esp_chip_model_t chip_model;
    uint32_t flash_size_bytes;
    size_t psram_size_bytes;
} board_profile_t;

/**
 * @brief Read the connected gateway target's chip and memory profile.
 *
 * @return ESP_OK when all target properties were read, or an ESP-IDF error.
 */
esp_err_t board_profile_read(board_profile_t *profile);
