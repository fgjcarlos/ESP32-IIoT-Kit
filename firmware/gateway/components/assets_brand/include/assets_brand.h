#pragma once

#include "esp_err.h"

/**
 * @brief Initialize the brand asset placeholder contract.
 *
 * Interface only — full implementation is owned by the future display wiring
 * change, gated on physical hardware verification.
 *
 * @return ESP_OK for the contract-only placeholder.
 *
 * @note The contract in this slice ships a single ASCII label constant
 *       (see ASSETS_BRAND_PLACEHOLDER_LABEL below). It does NOT decode any
 *       image format (no `esp_jpeg_*`, no `esp_png_*`, no LVGL image
 *       decoder), does NOT read from SPIFFS or FATFS, and does NOT allocate
 *       any draw buffer. Real brand asset bring-up belongs in a later Fase
 *       and is gated on the asset format decision plus physical carrier
 *       verification.
 */
esp_err_t assets_brand_init(void);

/**
 * @brief Brand placeholder label used by the boot splash and status screen.
 *
 * Defined here (header) rather than in the .c so future asset code can
 * `#include` it and reference a single source of truth for the placeholder
 * text. The value is intentionally ASCII-only and short so it survives any
 * display charset constraint the future implementation may pick.
 */
#define ASSETS_BRAND_PLACEHOLDER_LABEL "IIoT-Kit"

/* -------------------------------------------------------------------------
 * Scope of this contract slice.
 *
 * The future `assets_brand` component will own brand-related assets such as
 *   - product logo (image, format TBD: JPEG / PNG / raw RGB565 / LVGL img)
 *   - boot splash text
 *   - color palette (background, foreground, accent)
 *   - typography hints (font selection)
 *
 * None of those are introduced in this slice. This slice only carries:
 *   - one ASCII label constant (ASSETS_BRAND_PLACEHOLDER_LABEL), and
 *   - one init function that logs the label and returns ESP_OK.
 *
 * The future asset work is gated on:
 *   - the asset format decision (deferred; no format is committed yet),
 *   - storage backend decision (SPIFFS / FATFS on TF card / embedded in
 *     flash), and
 *   - physical carrier verification.
 *
 * The SPI bus shared constraint from `lvgl_port.h` and `lcd_driver.h` does
 * NOT apply here: this contract does not touch the bus, does not allocate
 * any buffer, and does not need any peripheral. The constraint is
 * acknowledged but unapplicable at the contract level.
 * ------------------------------------------------------------------------- */