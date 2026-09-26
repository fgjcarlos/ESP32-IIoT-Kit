#pragma once

#include <stdint.h>

#include "esp_err.h"

/**
 * @brief Encode one RGB color triplet in the WS2812 wire-order byte sequence.
 *
 * @param red Red intensity.
 * @param green Green intensity.
 * @param blue Blue intensity.
 * @param grb Output buffer for the three WS2812 GRB bytes.
 * @return ESP_OK on success or ESP_ERR_INVALID_ARG when @p grb is NULL.
 */
esp_err_t board_rgb_encode_grb(uint8_t red, uint8_t green, uint8_t blue, uint8_t grb[3]);

/**
 * @brief Run a one-shot low-brightness onboard GPIO48 RGB smoke sequence.
 *
 * Sends red, green, blue, and off in that order using the WS2812 RMT encoder.
 *
 * @return ESP_OK on success or an ESP-IDF error.
 */
esp_err_t board_rgb_run_smoke_test(void);
