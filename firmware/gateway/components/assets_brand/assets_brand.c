/**
 * @file assets_brand.c
 * @brief Brand assets of the active profile: logo, palette and splash line.
 *
 * The assets themselves are selected and embedded by the build (the
 * `brand_profile` component owns the profile and the flash budget); this
 * component adapts them to what the local display needs.
 *
 * Until the LVGL port lands (issue #10 shipped the contract only), the
 * splash line is published as text and the logo as raw RGB565 pixels. No
 * image is decoded and no draw buffer is allocated here.
 */

#include "assets_brand.h"

#include <string.h>

#include "brand_profile.h"
#include "esp_log.h"

static const char *TAG = "assets_brand";

/** Pack a 0xRRGGBB colour into RGB565 (5 bits red, 6 green, 5 blue). */
static uint16_t assets_brand_to_rgb565(uint32_t rgb)
{
    const uint16_t red = (uint16_t)(((rgb >> 16) & 0xF8u) << 8);
    const uint16_t green = (uint16_t)(((rgb >> 8) & 0xFCu) << 3);
    const uint16_t blue = (uint16_t)((rgb & 0xFFu) >> 3);

    return (uint16_t)(red | green | blue);
}

esp_err_t assets_brand_init(void)
{
    const brand_profile_t *profile = brand_profile_get();

    const esp_err_t err = brand_profile_validate(profile);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Invalid brand profile '%s': %s",
                 profile->profile_id, esp_err_to_name(err));
        return err;
    }

    ESP_LOGI(TAG,
             "brand assets: profile=%s logo=%ux%u (%u bytes) palette=%06X/%06X/%06X",
             profile->profile_id,
             (unsigned int)profile->logo_width,
             (unsigned int)profile->logo_height,
             (unsigned int)profile->logo_size,
             (unsigned int)profile->color_background,
             (unsigned int)profile->color_foreground,
             (unsigned int)profile->color_accent);

    return ESP_OK;
}

const uint8_t *assets_brand_logo(uint16_t *width, uint16_t *height, size_t *size)
{
    const brand_profile_t *profile = brand_profile_get();

    if (width != NULL) {
        *width = profile->logo_width;
    }
    if (height != NULL) {
        *height = profile->logo_height;
    }
    if (size != NULL) {
        *size = profile->logo_size;
    }

    return profile->logo;
}

bool assets_brand_has_logo(void)
{
    const brand_profile_t *profile = brand_profile_get();
    return (profile->logo != NULL) && (profile->logo_size > 0u);
}

esp_err_t assets_brand_get_palette(assets_brand_palette_t *out)
{
    if (out == NULL) {
        return ESP_ERR_INVALID_ARG;
    }

    const brand_profile_t *profile = brand_profile_get();

    out->background = assets_brand_to_rgb565(profile->color_background);
    out->foreground = assets_brand_to_rgb565(profile->color_foreground);
    out->accent = assets_brand_to_rgb565(profile->color_accent);

    return ESP_OK;
}

esp_err_t assets_brand_splash_text(char *out, size_t out_len)
{
    if (out == NULL) {
        return ESP_ERR_INVALID_ARG;
    }

    return brand_profile_summary(brand_profile_get(), out, out_len);
}
