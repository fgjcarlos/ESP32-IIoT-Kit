/**
 * @file brand_profile_esp.c
 * @brief ESP-IDF half of the brand profile layer.
 *
 * Reads the public identity from the Kconfig values, resolves the logo
 * embedded by the build and the build revision, and logs the identity at
 * boot. The manufacturing secrets are read from the gitignored
 * `brand_secrets.local.h` when the build injected them.
 *
 * The deterministic logic lives in `brand_profile.c`; this file only glues
 * the compiled configuration to the portable API.
 */

#include "brand_profile.h"

#include <string.h>

#include "esp_log.h"
#include "sdkconfig.h"

#if defined(__has_include)
#  if __has_include("brand_build_info.h")
#    include "brand_build_info.h"
#  endif
#endif

#if defined(__has_include)
#  if __has_include("brand_secrets.local.h")
#    include "brand_secrets.local.h"
#  endif
#endif

/* Fallbacks for builds without the generated header (the host test build). */
#ifndef BRAND_BUILD_PROFILE_ID
#define BRAND_BUILD_PROFILE_ID "unknown"
#endif
#ifndef BRAND_BUILD_REVISION
#define BRAND_BUILD_REVISION "unknown"
#endif
#ifndef BRAND_LOGO_WIDTH
#define BRAND_LOGO_WIDTH 0u
#endif
#ifndef BRAND_LOGO_HEIGHT
#define BRAND_LOGO_HEIGHT 0u
#endif
#ifndef BRAND_ASSET_BYTES
#define BRAND_ASSET_BYTES 0u
#endif
#ifndef BRAND_ASSET_BUDGET_BYTES
#define BRAND_ASSET_BUDGET_BYTES 0u
#endif

/* The logo is embedded by the build with a stable symbol name, so the
 * profile can change without touching this file. The build emits the size
 * of the blob next to it, which avoids doing pointer arithmetic on the
 * start and end labels. */
#if defined(BRAND_PROFILE_EMBEDDED_LOGO) && BRAND_PROFILE_EMBEDDED_LOGO
extern const uint8_t _binary_brand_logo_start[];
extern const uint32_t brand_logo_length;
#endif

static const char *TAG = "brand_profile";

static brand_profile_t s_profile;
static bool s_profile_ready;

static const brand_secrets_t s_secrets = {
#ifdef BRAND_SECRET_PROVISIONING_KEY
    .provisioning_key = BRAND_SECRET_PROVISIONING_KEY,
#else
    .provisioning_key = NULL,
#endif
#ifdef BRAND_SECRET_SUPPORT_TOKEN
    .support_token = BRAND_SECRET_SUPPORT_TOKEN,
#else
    .support_token = NULL,
#endif
#ifdef BRAND_SECRET_AP_PASSWORD
    .ap_password = BRAND_SECRET_AP_PASSWORD,
#else
    .ap_password = NULL,
#endif
};

static void brand_profile_init_once(void)
{
    if (s_profile_ready) {
        return;
    }

    s_profile.profile_id = BRAND_BUILD_PROFILE_ID;
    s_profile.brand_name = CONFIG_BRAND_NAME;
    s_profile.model = CONFIG_BRAND_MODEL;
    s_profile.support_url = CONFIG_BRAND_SUPPORT_URL;
    s_profile.ap_ssid = CONFIG_BRAND_AP_SSID;
    s_profile.firmware_version = CONFIG_BRAND_FIRMWARE_VERSION;
    s_profile.ota_channel = CONFIG_BRAND_OTA_CHANNEL;
    s_profile.revision = BRAND_BUILD_REVISION;

    if (brand_profile_parse_color(CONFIG_BRAND_COLOR_BACKGROUND,
                                  &s_profile.color_background) != ESP_OK) {
        ESP_LOGE(TAG, "Invalid CONFIG_BRAND_COLOR_BACKGROUND");
    }
    if (brand_profile_parse_color(CONFIG_BRAND_COLOR_FOREGROUND,
                                  &s_profile.color_foreground) != ESP_OK) {
        ESP_LOGE(TAG, "Invalid CONFIG_BRAND_COLOR_FOREGROUND");
    }
    if (brand_profile_parse_color(CONFIG_BRAND_COLOR_ACCENT,
                                  &s_profile.color_accent) != ESP_OK) {
        ESP_LOGE(TAG, "Invalid CONFIG_BRAND_COLOR_ACCENT");
    }

#if defined(BRAND_PROFILE_EMBEDDED_LOGO) && BRAND_PROFILE_EMBEDDED_LOGO
    s_profile.logo = _binary_brand_logo_start;
    s_profile.logo_size = (size_t)brand_logo_length;
#else
    s_profile.logo = NULL;
    s_profile.logo_size = 0u;
#endif
    s_profile.logo_width = (uint16_t)BRAND_LOGO_WIDTH;
    s_profile.logo_height = (uint16_t)BRAND_LOGO_HEIGHT;

    s_profile_ready = true;
}

const brand_profile_t *brand_profile_get(void)
{
    brand_profile_init_once();
    return &s_profile;
}

const brand_secrets_t *brand_secrets_get(void)
{
    return &s_secrets;
}

bool brand_secrets_present(void)
{
    return (s_secrets.provisioning_key != NULL) ||
           (s_secrets.support_token != NULL) ||
           (s_secrets.ap_password != NULL);
}

esp_err_t brand_profile_log(void)
{
    const brand_profile_t *profile = brand_profile_get();

    const esp_err_t err = brand_profile_validate(profile);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Invalid brand profile '%s': %s",
                 profile->profile_id, esp_err_to_name(err));
        return err;
    }

    ESP_LOGI(TAG,
             "brand: name=%s model=%s version=%s revision=%s channel=%s",
             profile->brand_name, profile->model, profile->firmware_version,
             profile->revision, profile->ota_channel);

    ESP_LOGI(TAG,
             "brand: profile=%s support_url=%s ap_ssid=%s "
             "logo=%ux%u (%u bytes of a %u byte budget)",
             profile->profile_id, profile->support_url, profile->ap_ssid,
             (unsigned int)profile->logo_width,
             (unsigned int)profile->logo_height,
             (unsigned int)profile->logo_size,
             (unsigned int)BRAND_ASSET_BUDGET_BYTES);

    ESP_LOGI(TAG, "brand: manufacturing secrets %s",
             brand_secrets_present() ? "present" : "absent");

    return ESP_OK;
}
