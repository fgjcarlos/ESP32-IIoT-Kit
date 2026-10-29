/*
 * SPDX-License-Identifier: Apache-2.0
 *
 * Unit tests of the ST7789 diagnostic pattern.
 *
 * The same file runs on the host (tools/lcd-smoke/run-host.sh) and in an
 * ESP-IDF test project: it only uses the public interface of the component.
 */
#include <string.h>

#include "unity.h"

#include "lcd_smoke_pattern.h"

#define PATTERN_WIDTH LCD_SMOKE_PATTERN_WIDTH
#define PATTERN_HEIGHT LCD_SMOKE_PATTERN_HEIGHT
#define PATTERN_BYTES ((size_t)PATTERN_WIDTH * PATTERN_HEIGHT * 2u)
#define BORDER_PX LCD_SMOKE_PATTERN_BORDER_PX

/** @brief Byte written over the frame before a call that has to be rejected. */
#define SENTINEL 0x5Au

static uint8_t s_framebuffer[PATTERN_BYTES];

/**
 * @brief The colour the band at row @p y has to carry.
 *
 * The five bands are 64 rows tall each, from the top edge to the bottom
 * edge of the frame.
 */
static uint16_t expected_band_color(int y)
{
    if (y < 64) {
        return LCD_SMOKE_COLOR_RED;
    }
    if (y < 128) {
        return LCD_SMOKE_COLOR_GREEN;
    }
    if (y < 192) {
        return LCD_SMOKE_COLOR_BLUE;
    }
    if (y < 256) {
        return LCD_SMOKE_COLOR_WHITE;
    }
    return LCD_SMOKE_COLOR_BLACK;
}

/**
 * @brief Assert the two wire bytes of one pixel.
 */
static void assert_pixel(int x, int y, uint16_t expected)
{
    const size_t offset = ((size_t)y * PATTERN_WIDTH + (size_t)x) * 2u;
    TEST_ASSERT_EQUAL_HEX8((uint8_t)(expected >> 8), s_framebuffer[offset]);
    TEST_ASSERT_EQUAL_HEX8((uint8_t)(expected & 0xFFu), s_framebuffer[offset + 1]);
}

TEST_CASE("lcd smoke pattern paints the five bands with the exact colours", "[lcd-smoke][pattern]")
{
    memset(s_framebuffer, SENTINEL, sizeof(s_framebuffer));
    TEST_ESP_OK(lcd_smoke_pattern_fill(s_framebuffer, sizeof(s_framebuffer)));

    /*
     * Every pixel of every band is checked, not only a probe per band: the
     * border is the only thing allowed to cover a band pixel, and the bands
     * themselves have to be uniform from edge to edge.
     */
    for (int y = BORDER_PX; y < PATTERN_HEIGHT - BORDER_PX; y++) {
        const uint16_t expected = expected_band_color(y);
        for (int x = BORDER_PX; x < PATTERN_WIDTH - BORDER_PX; x++) {
            assert_pixel(x, y, expected);
        }
    }

    /* The band edges fall on exact multiples of 64 rows. */
    assert_pixel(PATTERN_WIDTH / 2, 63, LCD_SMOKE_COLOR_RED);
    assert_pixel(PATTERN_WIDTH / 2, 64, LCD_SMOKE_COLOR_GREEN);
    assert_pixel(PATTERN_WIDTH / 2, 127, LCD_SMOKE_COLOR_GREEN);
    assert_pixel(PATTERN_WIDTH / 2, 128, LCD_SMOKE_COLOR_BLUE);
    assert_pixel(PATTERN_WIDTH / 2, 191, LCD_SMOKE_COLOR_BLUE);
    assert_pixel(PATTERN_WIDTH / 2, 192, LCD_SMOKE_COLOR_WHITE);
    assert_pixel(PATTERN_WIDTH / 2, 255, LCD_SMOKE_COLOR_WHITE);
    assert_pixel(PATTERN_WIDTH / 2, 256, LCD_SMOKE_COLOR_BLACK);
}

TEST_CASE("lcd smoke pattern paints a thin contrasting border", "[lcd-smoke][pattern]")
{
    memset(s_framebuffer, SENTINEL, sizeof(s_framebuffer));
    TEST_ESP_OK(lcd_smoke_pattern_fill(s_framebuffer, sizeof(s_framebuffer)));

    /* The four corners and the four edges carry the border colour. */
    assert_pixel(0, 0, LCD_SMOKE_COLOR_BORDER);
    assert_pixel(PATTERN_WIDTH - 1, 0, LCD_SMOKE_COLOR_BORDER);
    assert_pixel(0, PATTERN_HEIGHT - 1, LCD_SMOKE_COLOR_BORDER);
    assert_pixel(PATTERN_WIDTH - 1, PATTERN_HEIGHT - 1, LCD_SMOKE_COLOR_BORDER);
    assert_pixel(PATTERN_WIDTH / 2, 0, LCD_SMOKE_COLOR_BORDER);
    assert_pixel(PATTERN_WIDTH / 2, PATTERN_HEIGHT - 1, LCD_SMOKE_COLOR_BORDER);
    assert_pixel(0, PATTERN_HEIGHT / 2, LCD_SMOKE_COLOR_BORDER);
    assert_pixel(PATTERN_WIDTH - 1, PATTERN_HEIGHT / 2, LCD_SMOKE_COLOR_BORDER);

    /* The border is exactly two pixels thick: the third pixel is the band. */
    assert_pixel(1, PATTERN_HEIGHT / 2, LCD_SMOKE_COLOR_BORDER);
    assert_pixel(2, PATTERN_HEIGHT / 2, LCD_SMOKE_COLOR_BLUE);
    assert_pixel(PATTERN_WIDTH - 3, PATTERN_HEIGHT / 2, LCD_SMOKE_COLOR_BLUE);
    assert_pixel(PATTERN_WIDTH / 2, 1, LCD_SMOKE_COLOR_BORDER);
    assert_pixel(PATTERN_WIDTH / 2, 2, LCD_SMOKE_COLOR_RED);
    assert_pixel(PATTERN_WIDTH / 2, PATTERN_HEIGHT - 3, LCD_SMOKE_COLOR_BLACK);
    assert_pixel(PATTERN_WIDTH / 2, PATTERN_HEIGHT - 2, LCD_SMOKE_COLOR_BORDER);

    /* The border wins over the band it covers. */
    assert_pixel(2, 2, LCD_SMOKE_COLOR_RED);
    assert_pixel(PATTERN_WIDTH - 3, 2, LCD_SMOKE_COLOR_RED);
    assert_pixel(2, PATTERN_HEIGHT - 3, LCD_SMOKE_COLOR_BLACK);
}

TEST_CASE("lcd smoke pattern writes the RGB565 wire bytes big endian first", "[lcd-smoke][pattern]")
{
    memset(s_framebuffer, SENTINEL, sizeof(s_framebuffer));
    TEST_ESP_OK(lcd_smoke_pattern_fill(s_framebuffer, sizeof(s_framebuffer)));

    /* Red is 0xF800: the high byte comes first on the wire. */
    const size_t red = ((size_t)32 * PATTERN_WIDTH + PATTERN_WIDTH / 2) * 2u;
    TEST_ASSERT_EQUAL_HEX8(0xF8, s_framebuffer[red]);
    TEST_ASSERT_EQUAL_HEX8(0x00, s_framebuffer[red + 1]);

    /* Blue is 0x001F: a zero high byte is still written, not skipped. */
    const size_t blue = ((size_t)160 * PATTERN_WIDTH + PATTERN_WIDTH / 2) * 2u;
    TEST_ASSERT_EQUAL_HEX8(0x00, s_framebuffer[blue]);
    TEST_ASSERT_EQUAL_HEX8(0x1F, s_framebuffer[blue + 1]);

    /* The frame starts with the border colour, 0xF81F, in the same order. */
    TEST_ASSERT_EQUAL_HEX8(0xF8, s_framebuffer[0]);
    TEST_ASSERT_EQUAL_HEX8(0x1F, s_framebuffer[1]);

    /* The frame ends with the border colour too. */
    TEST_ASSERT_EQUAL_HEX8(0xF8, s_framebuffer[PATTERN_BYTES - 2]);
    TEST_ASSERT_EQUAL_HEX8(0x1F, s_framebuffer[PATTERN_BYTES - 1]);
}

TEST_CASE("lcd smoke pattern rejects an unusable buffer and writes nothing", "[lcd-smoke][pattern]")
{
    memset(s_framebuffer, SENTINEL, sizeof(s_framebuffer));

    /* A null buffer is refused. */
    TEST_ESP_ERR(ESP_ERR_INVALID_ARG, lcd_smoke_pattern_fill(NULL, sizeof(s_framebuffer)));
    /* A buffer that cannot hold the frame is refused. */
    TEST_ESP_ERR(ESP_ERR_INVALID_SIZE, lcd_smoke_pattern_fill(s_framebuffer, PATTERN_BYTES - 2u));

    /* Every rejected call left the stored pixels untouched. */
    for (size_t i = 0; i < PATTERN_BYTES; i++) {
        TEST_ASSERT_EQUAL_HEX8(SENTINEL, s_framebuffer[i]);
    }
}

TEST_CASE("lcd smoke exact frame and bounds", "[lcd-smoke][pattern]")
{
    static uint8_t frame[108802];
    const uint16_t colors[] = {0xf800, 0x07e0, 0x001f, 0xffff, 0x0000};
    memset(frame, SENTINEL, sizeof(frame));
    TEST_ESP_OK(lcd_smoke_pattern_fill(frame + 1, 108801));

    for (int y = 0; y < 320; ++y) {
        for (int x = 0; x < 170; ++x) {
            const uint16_t want = (x < 2 || x >= 168 || y < 2 || y >= 318)
                ? 0xf81f : colors[y / 64];
            const size_t i = 1 + ((size_t)y * 170 + x) * 2;
            TEST_ASSERT_EQUAL_HEX8(want >> 8, frame[i]);
            TEST_ASSERT_EQUAL_HEX8(want & 255, frame[i + 1]);
        }
    }
    TEST_ASSERT_EQUAL_HEX8(SENTINEL, frame[0]);
    TEST_ASSERT_EQUAL_HEX8(SENTINEL, frame[108801]);
}

TEST_CASE("lcd smoke rejected sizes preserve all bytes", "[lcd-smoke][pattern]")
{
    const size_t sizes[] = {0, 1, 108798, 108799};
    memset(s_framebuffer, SENTINEL, sizeof(s_framebuffer));
    TEST_ESP_ERR(ESP_ERR_INVALID_ARG, lcd_smoke_pattern_fill(NULL, 0));
    TEST_ESP_ERR(ESP_ERR_INVALID_ARG, lcd_smoke_pattern_fill(NULL, sizeof(s_framebuffer)));

    for (size_t i = 0; i < sizeof(sizes) / sizeof(sizes[0]); ++i) {
        TEST_ESP_ERR(ESP_ERR_INVALID_SIZE, lcd_smoke_pattern_fill(s_framebuffer, sizes[i]));
        for (size_t byte = 0; byte < sizeof(s_framebuffer); ++byte) {
            TEST_ASSERT_EQUAL_HEX8(SENTINEL, s_framebuffer[byte]);
        }
    }
}
