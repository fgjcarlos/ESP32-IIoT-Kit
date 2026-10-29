/*
 * SPDX-License-Identifier: Apache-2.0
 *
 * ST7789 image diagnostic.
 *
 * Draws one fixed RGB565 frame on the 170x320 panel of the gateway board and
 * stops. It is a standalone ESP-IDF project, it does not share code with the
 * gateway firmware and it does not touch any gateway component.
 *
 * The program never reports that the panel shows the image: it only reports
 * that the frame was handed to the driver. Whether the panel lights up is
 * observed by a person in front of the board.
 */
#include <stdint.h>

#include "driver/spi_master.h"
#include "esp_check.h"
#include "esp_err.h"
#include "esp_heap_caps.h"
#include "esp_lcd_io_spi.h"
#include "esp_lcd_panel_io.h"
#include "esp_lcd_panel_ops.h"
#include "esp_lcd_panel_st7789.h"
#include "esp_log.h"
#include "sdkconfig.h"

#include "lcd_smoke_pattern.h"

#define LCD_SMOKE_TAG "lcd_smoke"

/*
 * Wiring used by the diagnostic. The authoritative map of the board lives in
 * docs/hardware/esp32-s3-wroom-1-n16r8.md; GPIO7 is not used here, the
 * backlight is left as the integrator wired it.
 */
#define LCD_SMOKE_PIN_MOSI 11
#define LCD_SMOKE_PIN_SCLK 12
#define LCD_SMOKE_PIN_DC 8
#define LCD_SMOKE_PIN_RST 9
#define LCD_SMOKE_PIN_CS 10

#define LCD_SMOKE_SPI_HOST SPI2_HOST
#define LCD_SMOKE_SPI_CLOCK_HZ (10 * 1000 * 1000)

/** @brief Conservative depth of the transaction queue of the panel IO. */
#define LCD_SMOKE_QUEUE_DEPTH 4

/** @brief Frame buffer, allocated once and kept until the board is reset. */
static uint8_t *s_framebuffer;

/**
 * @brief Paint the pattern, bring the panel up and hand the frame over.
 */
static esp_err_t lcd_smoke_run(void)
{
    esp_lcd_panel_io_handle_t panel_io = NULL;
    esp_lcd_panel_handle_t panel = NULL;

    const spi_bus_config_t bus_config = {
        .mosi_io_num = LCD_SMOKE_PIN_MOSI,
        .miso_io_num = -1,
        .sclk_io_num = LCD_SMOKE_PIN_SCLK,
        .quadwp_io_num = -1,
        .quadhd_io_num = -1,
        .data4_io_num = -1,
        .data5_io_num = -1,
        .data6_io_num = -1,
        .data7_io_num = -1,
        /*
         * The whole frame is offered as the limit of one transaction. The
         * SPI master reports the size the chip can really move in one go and
         * the ESP-IDF panel IO splits the colour transfer into chunks of that
         * size, keeping the chip select active between them.
         */
        .max_transfer_sz = LCD_SMOKE_PATTERN_BYTES,
    };
    ESP_ERROR_CHECK(spi_bus_initialize(LCD_SMOKE_SPI_HOST, &bus_config, SPI_DMA_CH_AUTO));

    const esp_lcd_panel_io_spi_config_t io_config = {
        .dc_gpio_num = LCD_SMOKE_PIN_DC,
        .cs_gpio_num = LCD_SMOKE_PIN_CS,
        .pclk_hz = LCD_SMOKE_SPI_CLOCK_HZ,
        .lcd_cmd_bits = 8,
        .lcd_param_bits = 8,
        /* Assumption, not a measured fact of the panel: SPI mode 0. */
        .spi_mode = 0,
        .trans_queue_depth = LCD_SMOKE_QUEUE_DEPTH,
    };
    ESP_ERROR_CHECK(esp_lcd_new_panel_io_spi((esp_lcd_spi_bus_handle_t)LCD_SMOKE_SPI_HOST, &io_config, &panel_io));

    const esp_lcd_panel_dev_config_t panel_config = {
        .reset_gpio_num = LCD_SMOKE_PIN_RST,
#if CONFIG_LCD_SMOKE_COLOR_ORDER_BGR
        .rgb_ele_order = LCD_RGB_ELEMENT_ORDER_BGR,
#else
        .rgb_ele_order = LCD_RGB_ELEMENT_ORDER_RGB,
#endif
        .data_endian = LCD_RGB_DATA_ENDIAN_BIG,
        .bits_per_pixel = 16,
        .flags = {
            /* Assumption, not a measured fact of the panel: active low reset. */
            .reset_active_high = 0,
        },
    };
    ESP_ERROR_CHECK(esp_lcd_new_panel_st7789(panel_io, &panel_config, &panel));

    /* Bring the controller up and apply the provisional settings. */
    ESP_ERROR_CHECK(esp_lcd_panel_reset(panel));
    ESP_ERROR_CHECK(esp_lcd_panel_init(panel));
    ESP_ERROR_CHECK(esp_lcd_panel_set_gap(panel, CONFIG_LCD_SMOKE_X_GAP, CONFIG_LCD_SMOKE_Y_GAP));
#if CONFIG_LCD_SMOKE_INVERT_COLOR
    ESP_ERROR_CHECK(esp_lcd_panel_invert_color(panel, true));
#else
    ESP_ERROR_CHECK(esp_lcd_panel_invert_color(panel, false));
#endif

    /* The pattern is written once, before the frame is handed to the driver. */
    ESP_ERROR_CHECK(lcd_smoke_pattern_fill(s_framebuffer, LCD_SMOKE_PATTERN_BYTES));

    ESP_ERROR_CHECK(esp_lcd_panel_disp_on_off(panel, true));
    ESP_ERROR_CHECK(esp_lcd_panel_draw_bitmap(panel,
                                              0,
                                              0,
                                              LCD_SMOKE_PATTERN_WIDTH,
                                              LCD_SMOKE_PATTERN_HEIGHT,
                                              s_framebuffer));

    ESP_LOGI(LCD_SMOKE_TAG, "frame queued for the panel, visual confirmation pending");
    return ESP_OK;
}

void app_main(void)
{
    /*
     * The frame buffer stays in internal memory that the DMA can read
     * directly, and it is never freed: the panel IO may still be sending it
     * when the program ends, so the buffer has to outlive this function.
     */
    s_framebuffer = heap_caps_malloc(LCD_SMOKE_PATTERN_BYTES, MALLOC_CAP_INTERNAL | MALLOC_CAP_DMA);
    if (s_framebuffer == NULL) {
        ESP_LOGE(LCD_SMOKE_TAG, "no memory for the %u byte frame buffer", (unsigned)LCD_SMOKE_PATTERN_BYTES);
        return;
    }

    ESP_ERROR_CHECK(lcd_smoke_run());
}
