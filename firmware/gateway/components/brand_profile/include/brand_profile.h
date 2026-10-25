#pragma once

/**
 * @file brand_profile.h
 * @brief White-label layer: the public identity of the product.
 *
 * A *brand profile* is a build-time selection. The Kconfig choice
 * `BRAND_PROFILE` picks one of the profiles under
 * `components/brand_profile/profiles/<id>/`, each of which owns its own
 * versioned assets (logo and manifest) and its own identity defaults.
 *
 * The point of the layer is that a customer gets a different product name,
 * a different logo, different colours and a different support URL *without*
 * a fork of the firmware: the same source tree produces both images.
 *
 * Two kinds of data live here and they must never be mixed:
 *
 *   - **Public configuration** — name, model, product identifier, hardware
 *     revision, support URL, initial SSID, OTA channel, colours. Authored in
 *     `Kconfig.projbuild`, versioned in Git, safe to publish.
 *   - **Manufacturing secrets** — provisioning key, support token and the
 *     factory AP password. Injected at build time from `/.env` through
 *     `tools/brand/gen-brand-secrets.sh` into the gitignored
 *     `brand_secrets.local.h`. Never in Git, never in a log line.
 *
 * The header is split in two halves:
 *
 *   - The *portable* half (this file plus `brand_profile.c`) is plain C and
 *     runs unchanged on the host, so the deterministic parts — colour
 *     parsing, validation, the JSON payload and the artifact naming — are
 *     unit-tested without hardware.
 *   - The *ESP-IDF* half (`brand_profile_esp.c`) reads the Kconfig values,
 *     resolves the embedded logo and logs the identity at boot.
 */

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

/** Maximum length of the brand name, including the terminator. */
#define BRAND_PROFILE_NAME_MAX_LEN        48u
/** Maximum length of the product model, including the terminator. */
#define BRAND_PROFILE_MODEL_MAX_LEN       32u
/** Maximum length of the product identifier, including the terminator. */
#define BRAND_PROFILE_PRODUCT_ID_MAX_LEN  48u
/** Maximum length of the support URL, including the terminator. */
#define BRAND_PROFILE_URL_MAX_LEN         128u
/** Maximum length of the initial soft-AP SSID, including the terminator. */
#define BRAND_PROFILE_SSID_MAX_LEN        32u
/** Maximum length of the OTA channel, including the terminator. */
#define BRAND_PROFILE_CHANNEL_MAX_LEN     16u
/** Maximum length of the build revision, including the terminator. */
#define BRAND_PROFILE_REVISION_MAX_LEN    16u
/** Maximum length of the hardware revision, including the terminator. */
#define BRAND_PROFILE_HW_REVISION_MAX_LEN 16u
/** Maximum length of the firmware version, including the terminator. */
#define BRAND_PROFILE_VERSION_MAX_LEN     32u
/** Maximum length of the profile identifier, including the terminator. */
#define BRAND_PROFILE_ID_MAX_LEN          32u
/** Maximum length of a colour written as "RRGGBB", including the terminator. */
#define BRAND_PROFILE_COLOR_STR_LEN       8u
/** Maximum length of the generated artifact name, including the terminator. */
#define BRAND_PROFILE_ARTIFACT_MAX_LEN   160u
/** Maximum length of the JSON payload, including the terminator. */
#define BRAND_PROFILE_JSON_MAX_LEN       768u

/**
 * @brief Public identity of the product, as compiled into the firmware.
 *
 * Every pointer refers to static storage owned by the component: the values
 * live for the whole lifetime of the process and must not be freed.
 */
typedef struct {
    const char *profile_id;       /**< Identifier of the compiled profile. */
    const char *brand_name;       /**< Brand name shown to the user. */
    const char *model;            /**< Commercial model of the product. */
    const char *product_id;       /**< Stable identifier of the product. */
    const char *support_url;      /**< Public support address. */
    const char *ap_ssid;          /**< Initial soft-AP SSID. */
    const char *firmware_version; /**< Version of this build. */
    const char *ota_channel;      /**< OTA release channel. */
    const char *revision;         /**< Build revision (short commit SHA). */
    const char *hw_revision;      /**< Hardware revision of the carrier. */
    uint32_t    color_background; /**< Background colour as 0xRRGGBB. */
    uint32_t    color_foreground; /**< Foreground colour as 0xRRGGBB. */
    uint32_t    color_accent;     /**< Accent colour as 0xRRGGBB. */
    const uint8_t *logo;          /**< Embedded logo, RGB565, or NULL. */
    size_t         logo_size;     /**< Size of the embedded logo in bytes. */
    uint16_t    logo_width;       /**< Logo width in pixels. */
    uint16_t    logo_height;      /**< Logo height in pixels. */
} brand_profile_t;

/**
 * @brief Manufacturing secrets injected at build time.
 *
 * All fields are NULL when the gitignored `brand_secrets.local.h` is absent
 * (the CI / developer case). A NULL field means "not provisioned", never
 * "empty value".
 */
typedef struct {
    const char *provisioning_key; /**< Per-unit provisioning secret. */
    const char *support_token;    /**< Token for the support API. */
    const char *ap_password;      /**< Factory password of the soft-AP. */
} brand_secrets_t;

/**
 * @brief Return the identity compiled into this firmware.
 *
 * The returned pointer is to static storage and stays valid for the whole
 * lifetime of the process.
 */
const brand_profile_t *brand_profile_get(void);

/**
 * @brief Return the manufacturing secrets compiled into this firmware.
 *
 * Returns a pointer to static storage whose fields are NULL when no secrets
 * were injected. Never logs or prints the values.
 */
const brand_secrets_t *brand_secrets_get(void);

/**
 * @brief Report whether any manufacturing secret was injected.
 */
bool brand_secrets_present(void);

/**
 * @brief Parse a "RRGGBB" colour string into a 0xRRGGBB integer.
 *
 * Accepts 6 hex digits, with an optional leading '#'. The comparison is
 * case-insensitive.
 *
 * @param hex     Colour string. May be NULL.
 * @param out_rgb Destination for the parsed colour. May not be NULL.
 * @return ESP_OK on success, ESP_ERR_INVALID_ARG for a NULL pointer or a
 *         malformed string, ESP_ERR_INVALID_SIZE for a wrong length.
 */
esp_err_t brand_profile_parse_color(const char *hex, uint32_t *out_rgb);

/**
 * @brief Format a 0xRRGGBB colour as an uppercase "RRGGBB" string.
 *
 * @param rgb     Colour value.
 * @param out     Destination buffer.
 * @param out_len Size of @p out. Must be at least BRAND_PROFILE_COLOR_STR_LEN.
 * @return ESP_OK on success, ESP_ERR_INVALID_ARG on a NULL pointer,
 *         ESP_ERR_INVALID_SIZE when the buffer is too small.
 */
esp_err_t brand_profile_format_color(uint32_t rgb, char *out, size_t out_len);

/**
 * @brief Check the consistency of a brand profile.
 *
 * Verifies that the identity fields are present and within their limits, that
 * the support URL is an absolute http(s) URL, that the SSID fits the 802.11
 * limit and that the logo, when present, matches its declared dimensions.
 *
 * @param profile Profile to check. May not be NULL.
 * @return ESP_OK when the profile is coherent, otherwise an ESP-IDF error.
 */
esp_err_t brand_profile_validate(const brand_profile_t *profile);

/**
 * @brief Build the JSON payload published by the brand endpoint.
 *
 * The output is deterministic: the same profile always yields byte-identical
 * JSON, so it can be asserted in a test and cached by a client.
 *
 * @param profile Profile to serialise. May not be NULL.
 * @param out     Destination buffer.
 * @param out_len Size of @p out.
 * @return ESP_OK on success, ESP_ERR_INVALID_ARG on a NULL pointer,
 *         ESP_ERR_INVALID_SIZE when the buffer is too small.
 */
esp_err_t brand_profile_format_json(const brand_profile_t *profile,
                                     char *out, size_t out_len);

/**
 * @brief Compute the exact size of the JSON payload, including the terminator.
 *
 * @param profile Profile to serialise. May not be NULL.
 * @return The required buffer size, or 0 when @p profile is NULL.
 */
size_t brand_profile_json_size(const brand_profile_t *profile);

/**
 * @brief Normalise a string into an artifact slug.
 *
 * Lowercases ASCII letters, keeps digits and dots, maps spaces, underscores
 * and any other character to '-', and collapses repeated separators. The
 * result never starts or ends with a separator.
 *
 * @param in      Input string. May not be NULL.
 * @param out     Destination buffer.
 * @param out_len Size of @p out.
 * @return ESP_OK on success, ESP_ERR_INVALID_ARG on a NULL pointer,
 *         ESP_ERR_INVALID_SIZE when the buffer is too small.
 */
esp_err_t brand_profile_slugify(const char *in, char *out, size_t out_len);

/**
 * @brief Build the reproducible artifact name of a build.
 *
 * The convention is:
 *
 *     <brand>-<model>-v<version>-<channel>-<revision>.bin
 *
 * Every token is slugified, so the name only contains lowercase ASCII
 * letters, digits, dots and dashes. The same profile and the same revision
 * always produce the same name.
 *
 * @param profile Profile to name. May not be NULL.
 * @param out     Destination buffer.
 * @param out_len Size of @p out.
 * @return ESP_OK on success, ESP_ERR_INVALID_ARG on a NULL pointer,
 *         ESP_ERR_INVALID_SIZE when the buffer is too small.
 */
esp_err_t brand_profile_build_artifact_name(const brand_profile_t *profile,
                                            char *out, size_t out_len);

/**
 * @brief Build the one-line identity shown on the local display.
 *
 * Format: "<brand> <model> v<version> <revision>".
 *
 * @param profile Profile to summarise. May not be NULL.
 * @param out     Destination buffer.
 * @param out_len Size of @p out.
 * @return ESP_OK on success, ESP_ERR_INVALID_ARG on a NULL pointer,
 *         ESP_ERR_INVALID_SIZE when the buffer is too small.
 */
esp_err_t brand_profile_summary(const brand_profile_t *profile,
                                 char *out, size_t out_len);

/**
 * @brief Log the compiled identity at boot.
 *
 * Emits one line with the brand name, the model, the version, the revision,
 * the OTA channel, the support URL, the initial SSID and the asset report.
 * Secrets are never part of the output.
 *
 * @return ESP_OK on success, or an ESP-IDF error when the profile is invalid.
 */
esp_err_t brand_profile_log(void);

#ifdef __cplusplus
}
#endif
