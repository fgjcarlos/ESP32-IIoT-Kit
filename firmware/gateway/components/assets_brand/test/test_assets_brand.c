#include "unity.h"
#include "assets_brand.h"
#include "brand_profile.h"

#include <string.h>

TEST_CASE("Assets brand init returns ESP_OK", "[assets_brand]")
{
    TEST_ASSERT_EQUAL(ESP_OK, assets_brand_init());
}

TEST_CASE("Assets brand exposes the logo of the compiled profile", "[assets_brand]")
{
    uint16_t width = 0u;
    uint16_t height = 0u;
    size_t size = 0u;

    const uint8_t *logo = assets_brand_logo(&width, &height, &size);

    TEST_ASSERT_TRUE(assets_brand_has_logo());
    TEST_ASSERT_NOT_NULL(logo);
    TEST_ASSERT_TRUE(width > 0u);
    TEST_ASSERT_TRUE(height > 0u);

    /* RGB565: two bytes per pixel. */
    TEST_ASSERT_EQUAL_UINT32((uint32_t)width * (uint32_t)height * 2u,
                             (uint32_t)size);
}

TEST_CASE("Assets brand palette matches the brand profile colours", "[assets_brand]")
{
    assets_brand_palette_t palette;
    const brand_profile_t *profile = brand_profile_get();

    TEST_ASSERT_EQUAL(ESP_OK, assets_brand_get_palette(&palette));

    /* The display palette is the brand palette: the UI never invents a
     * colour of its own. The expected value is computed from the profile,
     * so the assertion holds for every profile. */
    const uint16_t expected_background =
        (uint16_t)((((profile->color_background >> 16) & 0xF8u) << 8) |
                   (((profile->color_background >> 8) & 0xFCu) << 3) |
                   ((profile->color_background & 0xFFu) >> 3));
    const uint16_t expected_foreground =
        (uint16_t)((((profile->color_foreground >> 16) & 0xF8u) << 8) |
                   (((profile->color_foreground >> 8) & 0xFCu) << 3) |
                   ((profile->color_foreground & 0xFFu) >> 3));
    const uint16_t expected_accent =
        (uint16_t)((((profile->color_accent >> 16) & 0xF8u) << 8) |
                   (((profile->color_accent >> 8) & 0xFCu) << 3) |
                   ((profile->color_accent & 0xFFu) >> 3));

    TEST_ASSERT_EQUAL_HEX16(expected_background, palette.background);
    TEST_ASSERT_EQUAL_HEX16(expected_foreground, palette.foreground);
    TEST_ASSERT_EQUAL_HEX16(expected_accent, palette.accent);

    TEST_ASSERT_EQUAL(ESP_ERR_INVALID_ARG, assets_brand_get_palette(NULL));
}

TEST_CASE("Assets brand splash line carries the product identity", "[assets_brand]")
{
    char splash[ASSETS_BRAND_SPLASH_MAX_LEN] = {0};
    const brand_profile_t *profile = brand_profile_get();

    TEST_ASSERT_EQUAL(ESP_OK, assets_brand_splash_text(splash, sizeof(splash)));

    /* The boot splash names the brand, the model, the version and the
     * revision: the identity of the product on the local display. */
    TEST_ASSERT_NOT_NULL(strstr(splash, profile->brand_name));
    TEST_ASSERT_NOT_NULL(strstr(splash, profile->model));
    TEST_ASSERT_NOT_NULL(strstr(splash, profile->firmware_version));
    TEST_ASSERT_NOT_NULL(strstr(splash, profile->revision));

    TEST_ASSERT_EQUAL(ESP_ERR_INVALID_SIZE,
                      assets_brand_splash_text(splash, 4u));
    TEST_ASSERT_EQUAL(ESP_ERR_INVALID_ARG, assets_brand_splash_text(NULL, 0u));
}
