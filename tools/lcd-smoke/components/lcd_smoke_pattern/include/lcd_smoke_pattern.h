/*
 * SPDX-License-Identifier: Apache-2.0
 *
 * Colour pattern of the ST7789 image diagnostic.
 *
 * The pattern is fixed: a 170x320 RGB565 frame with five horizontal bands and
 * a two pixel border. It owns no state and no memory, so it can be unit
 * tested on the host without a board.
 */
#pragma once

#include <stddef.h>
#include <stdint.h>

#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

/** @brief Width of the panel the diagnostic drives, in pixels. */
#define LCD_SMOKE_PATTERN_WIDTH 170
/** @brief Height of the panel the diagnostic drives, in pixels. */
#define LCD_SMOKE_PATTERN_HEIGHT 320
/** @brief Size of the RGB565 frame of the diagnostic, in bytes. */
#define LCD_SMOKE_PATTERN_BYTES (LCD_SMOKE_PATTERN_WIDTH * LCD_SMOKE_PATTERN_HEIGHT * 2u)
/** @brief Thickness of the border, in pixels, on the four edges. */
#define LCD_SMOKE_PATTERN_BORDER_PX 2

/** @brief Top band: red. */
#define LCD_SMOKE_COLOR_RED 0xF800u
/** @brief Second band: green. */
#define LCD_SMOKE_COLOR_GREEN 0x07E0u
/** @brief Third band: blue. */
#define LCD_SMOKE_COLOR_BLUE 0x001Fu
/** @brief Fourth band: white. */
#define LCD_SMOKE_COLOR_WHITE 0xFFFFu
/** @brief Bottom band: black. */
#define LCD_SMOKE_COLOR_BLACK 0x0000u
/** @brief Border colour: magenta, a colour none of the bands uses. */
#define LCD_SMOKE_COLOR_BORDER 0xF81Fu

/**
 * @brief Fill a frame buffer with the diagnostic pattern.
 *
 * The buffer receives the whole 170x320 frame in RGB565, with the high byte
 * of every pixel first, which is the byte order the controller expects after
 * the ESP-IDF ST7789 driver programs RAMCTRL with its big endian default.
 *
 * The five bands split the height of the frame, so each band covers exactly
 * 64 rows, from the top edge to the bottom edge. The border is painted over
 * the outermost pixels and therefore covers the first and the last rows of
 * the top and the bottom band.
 *
 * @param[out] buffer Frame buffer to fill. It must hold at least
 *                    LCD_SMOKE_PATTERN_BYTES bytes.
 * @param[in] buffer_size Size of @p buffer, in bytes.
 *
 * @return ESP_OK on success.
 * @return ESP_ERR_INVALID_ARG when @p buffer is NULL.
 * @return ESP_ERR_INVALID_SIZE when @p buffer is smaller than the frame.
 */
esp_err_t lcd_smoke_pattern_fill(uint8_t *buffer, size_t buffer_size);

#ifdef __cplusplus
}
#endif
