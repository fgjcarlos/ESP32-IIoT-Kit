/**
 * @file brand_profile.c
 * @brief Portable half of the brand profile layer.
 *
 * Everything in this file is plain C: no ESP-IDF call, no global state, no
 * allocation. That is what makes the interesting part of the white-label
 * layer — the deterministic bits — testable on the host
 * (`tools/host-tests`) as well as on the device.
 *
 * The ESP-IDF half (reading Kconfig, resolving the embedded logo, logging)
 * lives in `brand_profile_esp.c`.
 */

#include "brand_profile.h"

#include <stdio.h>
#include <string.h>

/** Bytes per pixel of the embedded logo (RGB565). */
#define BRAND_PROFILE_LOGO_BYTES_PER_PIXEL 2u

static int brand_hex_value(char c)
{
    if (c >= '0' && c <= '9') {
        return c - '0';
    }
    if (c >= 'a' && c <= 'f') {
        return 10 + (c - 'a');
    }
    if (c >= 'A' && c <= 'F') {
        return 10 + (c - 'A');
    }
    return -1;
}

static bool brand_is_separator(char c)
{
    return (c == ' ') || (c == '_') || (c == '\t') || (c == '-');
}

esp_err_t brand_profile_parse_color(const char *hex, uint32_t *out_rgb)
{
    if (hex == NULL || out_rgb == NULL) {
        return ESP_ERR_INVALID_ARG;
    }

    if (hex[0] == '#') {
        hex++;
    }

    size_t len = strnlen(hex, BRAND_PROFILE_COLOR_STR_LEN);
    if (len != 6u) {
        return ESP_ERR_INVALID_SIZE;
    }

    uint32_t rgb = 0u;
    for (size_t i = 0; i < 6u; i++) {
        const int digit = brand_hex_value(hex[i]);
        if (digit < 0) {
            return ESP_ERR_INVALID_ARG;
        }
        rgb = (rgb << 4) | (uint32_t)digit;
    }

    *out_rgb = rgb;
    return ESP_OK;
}

esp_err_t brand_profile_format_color(uint32_t rgb, char *out, size_t out_len)
{
    if (out == NULL) {
        return ESP_ERR_INVALID_ARG;
    }
    if (out_len < BRAND_PROFILE_COLOR_STR_LEN) {
        return ESP_ERR_INVALID_SIZE;
    }

    const int written = snprintf(out, out_len, "%06X",
                                 (unsigned int)(rgb & 0xFFFFFFu));
    if (written < 0 || (size_t)written >= out_len) {
        return ESP_ERR_INVALID_SIZE;
    }
    return ESP_OK;
}

esp_err_t brand_profile_validate(const brand_profile_t *profile)
{
    if (profile == NULL) {
        return ESP_ERR_INVALID_ARG;
    }

    if (profile->brand_name == NULL || profile->brand_name[0] == '\0') {
        return ESP_ERR_INVALID_ARG;
    }
    if (strlen(profile->brand_name) >= BRAND_PROFILE_NAME_MAX_LEN) {
        return ESP_ERR_INVALID_SIZE;
    }

    if (profile->model == NULL || profile->model[0] == '\0') {
        return ESP_ERR_INVALID_ARG;
    }
    if (strlen(profile->model) >= BRAND_PROFILE_MODEL_MAX_LEN) {
        return ESP_ERR_INVALID_SIZE;
    }

    if (profile->support_url == NULL || profile->support_url[0] == '\0') {
        return ESP_ERR_INVALID_ARG;
    }
    if (strlen(profile->support_url) >= BRAND_PROFILE_URL_MAX_LEN) {
        return ESP_ERR_INVALID_SIZE;
    }
    if (strncmp(profile->support_url, "http://", 7) != 0 &&
        strncmp(profile->support_url, "https://", 8) != 0) {
        return ESP_ERR_INVALID_ARG;
    }

    if (profile->ap_ssid == NULL || profile->ap_ssid[0] == '\0') {
        return ESP_ERR_INVALID_ARG;
    }
    if (strlen(profile->ap_ssid) >= BRAND_PROFILE_SSID_MAX_LEN) {
        return ESP_ERR_INVALID_SIZE;
    }

    if (profile->firmware_version == NULL || profile->firmware_version[0] == '\0') {
        return ESP_ERR_INVALID_ARG;
    }
    if (strlen(profile->firmware_version) >= BRAND_PROFILE_VERSION_MAX_LEN) {
        return ESP_ERR_INVALID_SIZE;
    }

    if (profile->ota_channel == NULL || profile->ota_channel[0] == '\0') {
        return ESP_ERR_INVALID_ARG;
    }
    if (strlen(profile->ota_channel) >= BRAND_PROFILE_CHANNEL_MAX_LEN) {
        return ESP_ERR_INVALID_SIZE;
    }

    if (profile->revision == NULL || profile->revision[0] == '\0') {
        return ESP_ERR_INVALID_ARG;
    }
    if (strlen(profile->revision) >= BRAND_PROFILE_REVISION_MAX_LEN) {
        return ESP_ERR_INVALID_SIZE;
    }

    if (profile->profile_id == NULL || profile->profile_id[0] == '\0') {
        return ESP_ERR_INVALID_ARG;
    }
    if (strlen(profile->profile_id) >= BRAND_PROFILE_ID_MAX_LEN) {
        return ESP_ERR_INVALID_SIZE;
    }

    /* A logo is either absent or exactly width * height RGB565 pixels. */
    if (profile->logo != NULL) {
        if (profile->logo_width == 0u || profile->logo_height == 0u) {
            return ESP_ERR_INVALID_ARG;
        }
        const size_t expected =
            (size_t)profile->logo_width * (size_t)profile->logo_height *
            BRAND_PROFILE_LOGO_BYTES_PER_PIXEL;
        if (profile->logo_size != expected) {
            return ESP_ERR_INVALID_SIZE;
        }
    }

    return ESP_OK;
}

size_t brand_profile_json_size(const brand_profile_t *profile)
{
    if (profile == NULL) {
        return 0u;
    }

    char background[BRAND_PROFILE_COLOR_STR_LEN] = {0};
    char foreground[BRAND_PROFILE_COLOR_STR_LEN] = {0};
    char accent[BRAND_PROFILE_COLOR_STR_LEN] = {0};

    if (brand_profile_format_color(profile->color_background, background,
                                   sizeof(background)) != ESP_OK ||
        brand_profile_format_color(profile->color_foreground, foreground,
                                   sizeof(foreground)) != ESP_OK ||
        brand_profile_format_color(profile->color_accent, accent,
                                   sizeof(accent)) != ESP_OK) {
        return 0u;
    }

    const int needed = snprintf(NULL, 0,
                                "{\"profile\":\"%s\",\"brand\":\"%s\","
                                "\"model\":\"%s\",\"version\":\"%s\","
                                "\"revision\":\"%s\",\"channel\":\"%s\","
                                "\"support_url\":\"%s\",\"ap_ssid\":\"%s\","
                                "\"colors\":{\"background\":\"%s\","
                                "\"foreground\":\"%s\",\"accent\":\"%s\"},"
                                "\"logo\":{\"width\":%u,\"height\":%u,"
                                "\"bytes\":%u}}",
                                profile->profile_id, profile->brand_name,
                                profile->model, profile->firmware_version,
                                profile->revision, profile->ota_channel,
                                profile->support_url, profile->ap_ssid,
                                background, foreground, accent,
                                (unsigned int)profile->logo_width,
                                (unsigned int)profile->logo_height,
                                (unsigned int)profile->logo_size);

    if (needed < 0) {
        return 0u;
    }
    return (size_t)needed + 1u;
}

esp_err_t brand_profile_format_json(const brand_profile_t *profile,
                                     char *out, size_t out_len)
{
    if (profile == NULL || out == NULL) {
        return ESP_ERR_INVALID_ARG;
    }

    char background[BRAND_PROFILE_COLOR_STR_LEN] = {0};
    char foreground[BRAND_PROFILE_COLOR_STR_LEN] = {0};
    char accent[BRAND_PROFILE_COLOR_STR_LEN] = {0};

    if (brand_profile_format_color(profile->color_background, background,
                                   sizeof(background)) != ESP_OK ||
        brand_profile_format_color(profile->color_foreground, foreground,
                                   sizeof(foreground)) != ESP_OK ||
        brand_profile_format_color(profile->color_accent, accent,
                                   sizeof(accent)) != ESP_OK) {
        return ESP_ERR_INVALID_SIZE;
    }

    const int written = snprintf(out, out_len,
                                 "{\"profile\":\"%s\",\"brand\":\"%s\","
                                 "\"model\":\"%s\",\"version\":\"%s\","
                                 "\"revision\":\"%s\",\"channel\":\"%s\","
                                 "\"support_url\":\"%s\",\"ap_ssid\":\"%s\","
                                 "\"colors\":{\"background\":\"%s\","
                                 "\"foreground\":\"%s\",\"accent\":\"%s\"},"
                                 "\"logo\":{\"width\":%u,\"height\":%u,"
                                 "\"bytes\":%u}}",
                                 profile->profile_id, profile->brand_name,
                                 profile->model, profile->firmware_version,
                                 profile->revision, profile->ota_channel,
                                 profile->support_url, profile->ap_ssid,
                                 background, foreground, accent,
                                 (unsigned int)profile->logo_width,
                                 (unsigned int)profile->logo_height,
                                 (unsigned int)profile->logo_size);

    if (written < 0) {
        return ESP_ERR_INVALID_ARG;
    }
    if ((size_t)written >= out_len) {
        return ESP_ERR_INVALID_SIZE;
    }
    return ESP_OK;
}

esp_err_t brand_profile_slugify(const char *in, char *out, size_t out_len)
{
    if (in == NULL || out == NULL) {
        return ESP_ERR_INVALID_ARG;
    }
    if (out_len == 0u) {
        return ESP_ERR_INVALID_SIZE;
    }

    size_t written = 0u;
    bool pending_separator = false;
    bool wrote_any = false;

    for (const char *p = in; *p != '\0'; p++) {
        const char c = *p;

        if (brand_is_separator(c)) {
            pending_separator = wrote_any;
            continue;
        }

        char lowered = c;
        if (c >= 'A' && c <= 'Z') {
            lowered = (char)(c - 'A' + 'a');
        }

        const bool allowed = (lowered >= 'a' && lowered <= 'z') ||
                             (lowered >= '0' && lowered <= '9') ||
                             (lowered == '.');
        if (!allowed) {
            pending_separator = wrote_any;
            continue;
        }

        if (pending_separator) {
            if (written + 1u >= out_len) {
                return ESP_ERR_INVALID_SIZE;
            }
            out[written++] = '-';
            pending_separator = false;
        }

        if (written + 1u >= out_len) {
            return ESP_ERR_INVALID_SIZE;
        }
        out[written++] = lowered;
        wrote_any = true;
    }

    out[written] = '\0';
    return ESP_OK;
}

esp_err_t brand_profile_build_artifact_name(const brand_profile_t *profile,
                                            char *out, size_t out_len)
{
    if (profile == NULL || out == NULL) {
        return ESP_ERR_INVALID_ARG;
    }

    char brand[BRAND_PROFILE_NAME_MAX_LEN] = {0};
    char model[BRAND_PROFILE_MODEL_MAX_LEN] = {0};
    char version[BRAND_PROFILE_VERSION_MAX_LEN] = {0};
    char channel[BRAND_PROFILE_CHANNEL_MAX_LEN] = {0};
    char revision[BRAND_PROFILE_REVISION_MAX_LEN] = {0};

    if (brand_profile_slugify(profile->brand_name, brand, sizeof(brand)) != ESP_OK ||
        brand_profile_slugify(profile->model, model, sizeof(model)) != ESP_OK ||
        brand_profile_slugify(profile->firmware_version, version, sizeof(version)) != ESP_OK ||
        brand_profile_slugify(profile->ota_channel, channel, sizeof(channel)) != ESP_OK ||
        brand_profile_slugify(profile->revision, revision, sizeof(revision)) != ESP_OK) {
        return ESP_ERR_INVALID_SIZE;
    }

    if (brand[0] == '\0' || model[0] == '\0' || version[0] == '\0' ||
        channel[0] == '\0' || revision[0] == '\0') {
        return ESP_ERR_INVALID_ARG;
    }

    const int written = snprintf(out, out_len, "%s-%s-v%s-%s-%s.bin",
                                 brand, model, version, channel, revision);
    if (written < 0) {
        return ESP_ERR_INVALID_ARG;
    }
    if ((size_t)written >= out_len) {
        return ESP_ERR_INVALID_SIZE;
    }
    return ESP_OK;
}

esp_err_t brand_profile_summary(const brand_profile_t *profile,
                                 char *out, size_t out_len)
{
    if (profile == NULL || out == NULL) {
        return ESP_ERR_INVALID_ARG;
    }

    const int written = snprintf(out, out_len, "%s %s v%s %s",
                                 profile->brand_name, profile->model,
                                 profile->firmware_version, profile->revision);
    if (written < 0) {
        return ESP_ERR_INVALID_ARG;
    }
    if ((size_t)written >= out_len) {
        return ESP_ERR_INVALID_SIZE;
    }
    return ESP_OK;
}
