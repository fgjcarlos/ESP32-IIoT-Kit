#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief The colours of the brand palette, ready for the display.
 *
 * The palette is the brand profile's palette; this structure only carries it
 * in the form the local UI needs.
 */
typedef struct {
    uint16_t background; /**< Background colour as RGB565. */
    uint16_t foreground; /**< Foreground (text) colour as RGB565. */
    uint16_t accent;     /**< Accent colour as RGB565. */
} assets_brand_palette_t;

/**
 * @brief Maximum length of the boot splash line, including the terminator.
 */
#define ASSETS_BRAND_SPLASH_MAX_LEN 96u

/**
 * @brief Initialize the brand assets.
 *
 * Resolves the logo embedded by the build for the active brand profile and
 * logs the asset report (logo size against the flash budget). The palette
 * comes from the same profile, so the display and the firmware never
 * disagree about the colours of the product.
 *
 * @return ESP_OK on success, or an ESP-IDF error when the brand profile is
 *         invalid.
 */
esp_err_t assets_brand_init(void);

/**
 * @brief Return the logo embedded for the active brand profile.
 *
 * @param width  Receives the logo width in pixels. May be NULL.
 * @param height Receives the logo height in pixels. May be NULL.
 * @param size   Receives the logo size in bytes. May be NULL.
 * @return The logo pixels as RGB565, or NULL when the profile carries no
 *         logo.
 */
const uint8_t *assets_brand_logo(uint16_t *width, uint16_t *height, size_t *size);

/**
 * @brief Return the brand palette converted to RGB565.
 *
 * @param out Receives the palette. May not be NULL.
 * @return ESP_OK on success, ESP_ERR_INVALID_ARG when @p out is NULL.
 */
esp_err_t assets_brand_get_palette(assets_brand_palette_t *out);

/**
 * @brief Build the boot splash line of the local display.
 *
 * Format: "<brand> <model> v<version> <revision>".
 *
 * @param out     Destination buffer.
 * @param out_len Size of @p out.
 * @return ESP_OK on success, ESP_ERR_INVALID_ARG on a NULL pointer,
 *         ESP_ERR_INVALID_SIZE when the buffer is too small.
 */
esp_err_t assets_brand_splash_text(char *out, size_t out_len);

/**
 * @brief Report whether a logo is embedded in this build.
 */
bool assets_brand_has_logo(void);

#ifdef __cplusplus
}
#endif
