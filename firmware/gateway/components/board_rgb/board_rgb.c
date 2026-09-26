#include <stdbool.h>
#include <stddef.h>

#include "board_rgb.h"
#include "driver/rmt_encoder.h"
#include "driver/rmt_tx.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#define BOARD_RGB_GPIO_NUM 48
#define BOARD_RGB_RMT_RESOLUTION_HZ (10U * 1000U * 1000U)
#define BOARD_RGB_COLOR_INTENSITY 8U
#define BOARD_RGB_STATE_HOLD_MS 1000U
#define BOARD_RGB_WS2812_PIXEL_BYTES 3U
#define BOARD_RGB_RESET_PHASE_DURATION_TICKS 250U
#define BOARD_RGB_ENCODER_MIN_SYMBOLS 25U

static const char *TAG = "board_rgb";

static const rmt_symbol_word_t ws2812_zero_symbol = {
    .duration0 = 3,
    .level0 = 1,
    .duration1 = 9,
    .level1 = 0,
};

static const rmt_symbol_word_t ws2812_one_symbol = {
    .duration0 = 9,
    .level0 = 1,
    .duration1 = 3,
    .level1 = 0,
};

static const rmt_symbol_word_t ws2812_reset_symbol = {
    .duration0 = BOARD_RGB_RESET_PHASE_DURATION_TICKS,
    .level0 = 0,
    .duration1 = BOARD_RGB_RESET_PHASE_DURATION_TICKS,
    .level1 = 0,
};

static size_t board_rgb_encode_ws2812(const void *data, size_t data_size,
                                      size_t symbols_written, size_t symbols_free,
                                      rmt_symbol_word_t *symbols, bool *done, void *arg)
{
    size_t symbol_count = 0;

    (void)arg;

    if (done == NULL) {
        return 0;
    }
    *done = false;

    if (data == NULL || symbols == NULL || data_size != BOARD_RGB_WS2812_PIXEL_BYTES ||
        symbols_written != 0 || symbols_free < BOARD_RGB_ENCODER_MIN_SYMBOLS) {
        *done = true;
        return 0;
    }

    const uint8_t *bytes = data;
    for (size_t byte_index = 0; byte_index < data_size; ++byte_index) {
        for (uint8_t bit_mask = 0x80; bit_mask != 0; bit_mask >>= 1) {
            symbols[symbol_count++] = (bytes[byte_index] & bit_mask) != 0 ?
                                        ws2812_one_symbol : ws2812_zero_symbol;
        }
    }
    symbols[symbol_count++] = ws2812_reset_symbol;

    *done = true;
    return symbol_count;
}

static esp_err_t board_rgb_transmit_color(rmt_channel_handle_t channel,
                                          rmt_encoder_handle_t encoder,
                                          uint8_t red, uint8_t green, uint8_t blue)
{
    uint8_t grb[3];
    const rmt_transmit_config_t transmit_config = {
        .loop_count = 0,
    };

    esp_err_t err = board_rgb_encode_grb(red, green, blue, grb);
    if (err != ESP_OK) {
        return err;
    }

    err = rmt_transmit(channel, encoder, grb, BOARD_RGB_WS2812_PIXEL_BYTES, &transmit_config);
    if (err != ESP_OK) {
        return err;
    }

    return rmt_tx_wait_all_done(channel, -1);
}

esp_err_t board_rgb_encode_grb(uint8_t red, uint8_t green, uint8_t blue, uint8_t grb[3])
{
    if (grb == NULL) {
        return ESP_ERR_INVALID_ARG;
    }

    grb[0] = green;
    grb[1] = red;
    grb[2] = blue;
    return ESP_OK;
}

esp_err_t board_rgb_run_smoke_test(void)
{
    const rmt_tx_channel_config_t channel_config = {
        .clk_src = RMT_CLK_SRC_DEFAULT,
        .gpio_num = BOARD_RGB_GPIO_NUM,
        .mem_block_symbols = 64,
        .resolution_hz = BOARD_RGB_RMT_RESOLUTION_HZ,
        .trans_queue_depth = 1,
    };
    const rmt_simple_encoder_config_t encoder_config = {
        .callback = board_rgb_encode_ws2812,
        .arg = NULL,
        .min_chunk_size = BOARD_RGB_ENCODER_MIN_SYMBOLS,
    };
    rmt_channel_handle_t channel = NULL;
    rmt_encoder_handle_t encoder = NULL;
    bool channel_enabled = false;
    esp_err_t err;

    ESP_LOGI(TAG,
             "Observe onboard LED: low-brightness GPIO48 RGB smoke red, green, blue, off");

    err = rmt_new_tx_channel(&channel_config, &channel);
    if (err != ESP_OK) {
        goto cleanup;
    }

    err = rmt_new_simple_encoder(&encoder_config, &encoder);
    if (err != ESP_OK) {
        goto cleanup;
    }

    err = rmt_enable(channel);
    if (err != ESP_OK) {
        goto cleanup;
    }
    channel_enabled = true;

    err = board_rgb_transmit_color(channel, encoder, BOARD_RGB_COLOR_INTENSITY, 0, 0);
    if (err != ESP_OK) {
        goto cleanup;
    }
    vTaskDelay(pdMS_TO_TICKS(BOARD_RGB_STATE_HOLD_MS));

    err = board_rgb_transmit_color(channel, encoder, 0, BOARD_RGB_COLOR_INTENSITY, 0);
    if (err != ESP_OK) {
        goto cleanup;
    }
    vTaskDelay(pdMS_TO_TICKS(BOARD_RGB_STATE_HOLD_MS));

    err = board_rgb_transmit_color(channel, encoder, 0, 0, BOARD_RGB_COLOR_INTENSITY);
    if (err != ESP_OK) {
        goto cleanup;
    }
    vTaskDelay(pdMS_TO_TICKS(BOARD_RGB_STATE_HOLD_MS));

    err = board_rgb_transmit_color(channel, encoder, 0, 0, 0);
    if (err != ESP_OK) {
        goto cleanup;
    }
    vTaskDelay(pdMS_TO_TICKS(BOARD_RGB_STATE_HOLD_MS));

cleanup:
    if (channel_enabled) {
        const esp_err_t disable_err = rmt_disable(channel);
        if (err == ESP_OK && disable_err != ESP_OK) {
            err = disable_err;
        }
    }
    if (encoder != NULL) {
        const esp_err_t delete_encoder_err = rmt_del_encoder(encoder);
        if (err == ESP_OK && delete_encoder_err != ESP_OK) {
            err = delete_encoder_err;
        }
    }
    if (channel != NULL) {
        const esp_err_t delete_channel_err = rmt_del_channel(channel);
        if (err == ESP_OK && delete_channel_err != ESP_OK) {
            err = delete_channel_err;
        }
    }

    return err;
}
