/*
 * SPDX-License-Identifier: Apache-2.0
 *
 * Colour pattern of the ST7789 image diagnostic.
 *
 * The pattern is fixed: five horizontal RGB565 bands with a two pixel border
 * over the outer edge. It owns no state and no memory, so it can be unit
 * tested on the host without a board.
 */
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <string.h>

#include "esp_err.h"

#include "lcd_smoke_pattern.h"

/** @brief Number of horizontal bands the height of the frame is split into. */
#define BAND_COUNT 5

/**
 * @brief Write one pixel, high byte first.
 *
 * The ESP-IDF ST7789 driver leaves the controller in big endian RAM mode, so
 * the frame is stored with the high byte of every pixel first.
 */
static void put_pixel(uint8_t *buffer, size_t index, uint16_t color)
{
    buffer[index * 2u] = (uint8_t)(color >> 8);
    buffer[index * 2u + 1u] = (uint8_t)(color & 0xFFu);
}

esp_err_t lcd_smoke_pattern_fill(uint8_t *buffer, size_t buffer_size)
{
    if (buffer == NULL) {
        return ESP_ERR_INVALID_ARG;
    }
    if (buffer_size < LCD_SMOKE_PATTERN_BYTES) {
        return ESP_ERR_INVALID_SIZE;
    }

    const int band_height = LCD_SMOKE_PATTERN_HEIGHT / BAND_COUNT;

    for (int y = 0; y < LCD_SMOKE_PATTERN_HEIGHT; y++) {
        uint16_t band_color;
        switch (y / band_height) {
        case 0:
            band_color = LCD_SMOKE_COLOR_RED;
            break;
        case 1:
            band_color = LCD_SMOKE_COLOR_GREEN;
            break;
        case 2:
            band_color = LCD_SMOKE_COLOR_BLUE;
            break;
        case 3:
            band_color = LCD_SMOKE_COLOR_WHITE;
            break;
        default:
            band_color = LCD_SMOKE_COLOR_BLACK;
            break;
        }

        for (int x = 0; x < LCD_SMOKE_PATTERN_WIDTH; x++) {
            const bool on_border = (x < LCD_SMOKE_PATTERN_BORDER_PX) ||
                                   (x >= LCD_SMOKE_PATTERN_WIDTH - LCD_SMOKE_PATTERN_BORDER_PX) ||
                                   (y < LCD_SMOKE_PATTERN_BORDER_PX) ||
                                   (y >= LCD_SMOKE_PATTERN_HEIGHT - LCD_SMOKE_PATTERN_BORDER_PX);
            const size_t index = (size_t)y * LCD_SMOKE_PATTERN_WIDTH + (size_t)x;
            put_pixel(buffer, index, on_border ? LCD_SMOKE_COLOR_BORDER : band_color);
        }
    }

    return ESP_OK;
}
