#pragma once

/**
 * @file ota_manifest.h
 * @brief Signed release manifest of an OTA image: model, parsing and rules.
 *
 * A *release manifest* is the document that authorises one firmware image.
 * It answers four questions at once:
 *
 *   - **What** the image is — product, brand, model, hardware revision,
 *     target chip and version.
 *   - **Where** it comes from — the release channel and the download URL.
 *   - **How much** it weighs — the exact size in bytes.
 *   - **Whether to trust it** — the SHA-256 of the image and an ECDSA
 *     signature over the canonical form of the manifest itself.
 *
 * The module is deliberately portable: plain C, no ESP-IDF call, no global
 * state, no allocation. The rules that decide whether an image may be
 * installed — compatibility, channel, anti-downgrade — are the interesting
 * part, so they live here where they can be executed on the host
 * (`tools/host-tests`) as well as on the device.
 *
 * The cryptographic primitives (SHA-256, ECDSA P-256) are *not* portable
 * across the two environments, so they enter through two function pointers
 * (`ota_digest_fn`, `ota_signature_verify_fn`). The ESP-IDF half provides the
 * mbedTLS implementations; the host tests provide deterministic doubles.
 *
 * The canonical form is the byte string that gets signed. It is defined
 * here, in one place, so the release tooling and the firmware cannot
 * disagree about what was signed:
 *
 *     iiot-ota-manifest/v1\n
 *     schema=<n>\n
 *     product=<product>\n
 *     brand=<brand>\n
 *     model=<model>\n
 *     revision=<revision>\n
 *     target=<target>\n
 *     version=<version>\n
 *     channel=<channel>\n
 *     size=<bytes>\n
 *     sha256=<64 lowercase hex>\n
 *     url=<url>\n
 *     key_id=<key id>\n
 *
 * Every line ends with a newline, the field order is fixed and no value may
 * contain a line break. The parser rejects those values, so the canonical
 * form is unambiguous.
 */

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

/** Version of the manifest schema this module understands. */
#define OTA_MANIFEST_SCHEMA 1u

/** Maximum length of the product identifier, including the terminator. */
#define OTA_MANIFEST_PRODUCT_MAX_LEN   48u
/** Maximum length of the brand name, including the terminator. */
#define OTA_MANIFEST_BRAND_MAX_LEN     48u
/** Maximum length of the product model, including the terminator. */
#define OTA_MANIFEST_MODEL_MAX_LEN     32u
/** Maximum length of the hardware revision, including the terminator. */
#define OTA_MANIFEST_REVISION_MAX_LEN  16u
/** Maximum length of the target chip name, including the terminator. */
#define OTA_MANIFEST_TARGET_MAX_LEN    16u
/** Maximum length of the firmware version, including the terminator. */
#define OTA_MANIFEST_VERSION_MAX_LEN   32u
/** Maximum length of the release channel, including the terminator. */
#define OTA_MANIFEST_CHANNEL_MAX_LEN   16u
/** Maximum length of the download URL, including the terminator. */
#define OTA_MANIFEST_URL_MAX_LEN       256u
/** Maximum length of the signing key identifier, including the terminator. */
#define OTA_MANIFEST_KEY_ID_MAX_LEN    32u
/** Maximum length of the base64 signature, including the terminator. */
#define OTA_MANIFEST_SIGNATURE_MAX_LEN 96u
/** Length of a SHA-256 digest, in bytes. */
#define OTA_MANIFEST_HASH_LEN          32u
/** Length of a SHA-256 digest written as hexadecimal, without the terminator. */
#define OTA_MANIFEST_HASH_HEX_LEN      64u
/** Maximum length of a manifest document, including the terminator. */
#define OTA_MANIFEST_JSON_MAX_LEN      2048u
/** Maximum length of the canonical payload, including the terminator. */
#define OTA_MANIFEST_PAYLOAD_MAX_LEN   1024u

/**
 * @brief The release manifest of one firmware image.
 *
 * Every string is NUL-terminated and bounded by the `*_MAX_LEN` constants.
 * The SHA-256 digests are stored as lowercase hexadecimal.
 */
typedef struct {
    uint32_t schema;                            /**< Schema version of the manifest. */
    char     product[OTA_MANIFEST_PRODUCT_MAX_LEN];   /**< Stable product identifier. */
    char     brand[OTA_MANIFEST_BRAND_MAX_LEN];       /**< Brand that owns the product. */
    char     model[OTA_MANIFEST_MODEL_MAX_LEN];       /**< Commercial model. */
    char     revision[OTA_MANIFEST_REVISION_MAX_LEN]; /**< Hardware revision of the carrier. */
    char     target[OTA_MANIFEST_TARGET_MAX_LEN];     /**< Target chip, e.g. "esp32s3". */
    char     version[OTA_MANIFEST_VERSION_MAX_LEN];   /**< Firmware version of the image. */
    char     channel[OTA_MANIFEST_CHANNEL_MAX_LEN];   /**< Release channel of the image. */
    uint32_t size;                              /**< Size of the image in bytes. */
    char     sha256[OTA_MANIFEST_HASH_HEX_LEN + 1u];  /**< SHA-256 of the image, lowercase hex. */
    char     url[OTA_MANIFEST_URL_MAX_LEN];            /**< HTTPS URL of the image. */
    char     signature[OTA_MANIFEST_SIGNATURE_MAX_LEN];/**< ECDSA signature, base64. */
    char     key_id[OTA_MANIFEST_KEY_ID_MAX_LEN];      /**< Identifier of the signing key. */
    char     manifest_sha256[OTA_MANIFEST_HASH_HEX_LEN + 1u]; /**< Optional digest of the canonical form. */
} ota_manifest_t;

/**
 * @brief The identity of the device that is considering an update.
 *
 * The caller fills it from the sources that already own those facts: the
 * brand profile (product, brand, model, hardware revision, channel) and the
 * board profile (target chip).
 */
typedef struct {
    const char *product;  /**< Stable product identifier of the device. */
    const char *brand;    /**< Brand name of the device. */
    const char *model;    /**< Commercial model of the device. */
    const char *revision; /**< Hardware revision of the device. */
    const char *target;   /**< Target chip of the device. */
    const char *version;  /**< Firmware version currently running. */
    const char *channel;  /**< Release channel the device belongs to. */
} ota_device_identity_t;

/**
 * @brief Why an image was refused.
 *
 * The reason travels with the error so the logs, and later the REST API,
 * can tell a failed download from a refused image.
 */
typedef enum {
    OTA_REJECT_NONE = 0,          /**< No rejection. */
    OTA_REJECT_MALFORMED,         /**< The manifest is not a valid document. */
    OTA_REJECT_SCHEMA,            /**< The manifest schema is not supported. */
    OTA_REJECT_SIGNATURE,         /**< The signature does not verify. */
    OTA_REJECT_MANIFEST_HASH,     /**< The digest of the manifest does not match. */
    OTA_REJECT_PRODUCT,           /**< The product identifier does not match. */
    OTA_REJECT_BRAND,             /**< The brand does not match. */
    OTA_REJECT_MODEL,             /**< The model does not match. */
    OTA_REJECT_REVISION,          /**< The hardware revision does not match. */
    OTA_REJECT_TARGET,            /**< The target chip does not match. */
    OTA_REJECT_CHANNEL,           /**< The release channel is not admissible. */
    OTA_REJECT_DOWNGRADE,         /**< The image is older than the running firmware. */
    OTA_REJECT_IMAGE_HASH,        /**< The image does not match the declared digest. */
    OTA_REJECT_IMAGE_SIZE,        /**< The image size is not admissible. */
} ota_reject_reason_t;

/**
 * @brief Stability rank of a release channel.
 *
 * The rank orders the channels from the least to the most stable. A device
 * only accepts images whose channel is at least as stable as its own, so a
 * development image can never reach a production unit.
 */
typedef enum {
    OTA_CHANNEL_UNKNOWN = -1, /**< The channel name is not recognised. */
    OTA_CHANNEL_DEV = 0,      /**< Development channel. */
    OTA_CHANNEL_BETA = 1,     /**< Beta channel. */
    OTA_CHANNEL_PRODUCTION = 2, /**< Production channel. */
} ota_channel_rank_t;

/**
 * @brief Digest callback: SHA-256 of a byte range.
 *
 * @param data Bytes to digest. May not be NULL unless @p len is zero.
 * @param len  Number of bytes.
 * @param out  Destination digest. May not be NULL.
 * @return ESP_OK on success.
 */
typedef esp_err_t (*ota_digest_fn)(const void *data, size_t len,
                                    uint8_t out[OTA_MANIFEST_HASH_LEN]);

/**
 * @brief Signature verification callback.
 *
 * The signature is the raw `r || s` pair of an ECDSA P-256 signature, each
 * half 32 bytes big, as it is carried inside the manifest. The data is the
 * canonical payload of the manifest.
 *
 * @param signature      Signature bytes. May not be NULL.
 * @param signature_len  Length of @p signature.
 * @param data           Signed bytes. May not be NULL.
 * @param data_len       Length of @p data.
 * @return ESP_OK when the signature is valid.
 */
typedef esp_err_t (*ota_signature_verify_fn)(const uint8_t *signature,
                                              size_t signature_len,
                                              const uint8_t *data,
                                              size_t data_len);

/**
 * @brief Parse a manifest document.
 *
 * The document is a flat JSON object. Every field of the schema is
 * mandatory, unknown fields are rejected, and every value must fit its
 * bound. A manifest without a `manifest_sha256` is accepted; when the field
 * is present it must hold a valid digest.
 *
 * @param json     Document bytes. May not be NULL.
 * @param json_len Length of @p json.
 * @param out      Destination manifest. May not be NULL.
 * @param reason   Optional destination for the rejection reason.
 * @return ESP_OK on success, otherwise an ESP-IDF error.
 */
esp_err_t ota_manifest_parse(const char *json, size_t json_len,
                             ota_manifest_t *out, ota_reject_reason_t *reason);

/**
 * @brief Build the canonical payload of a manifest.
 *
 * The payload is the exact byte string that the signature covers.
 *
 * @param manifest Manifest to serialise. May not be NULL.
 * @param out      Destination buffer.
 * @param out_len  Size of @p out.
 * @return ESP_OK on success, ESP_ERR_INVALID_ARG for a NULL pointer,
 *         ESP_ERR_INVALID_SIZE when the buffer is too small.
 */
esp_err_t ota_manifest_canonical_payload(const ota_manifest_t *manifest,
                                          char *out, size_t out_len);

/**
 * @brief Compute the exact size of the canonical payload, including the
 *        terminator.
 *
 * @param manifest Manifest to measure. May not be NULL.
 * @return The required size, or 0 when @p manifest is NULL.
 */
size_t ota_manifest_canonical_payload_size(const ota_manifest_t *manifest);

/**
 * @brief Digest the canonical payload of a manifest.
 *
 * @param manifest Manifest to digest. May not be NULL.
 * @param digest   Digest callback. May not be NULL.
 * @param out      Destination digest. May not be NULL.
 * @return ESP_OK on success.
 */
esp_err_t ota_manifest_digest(const ota_manifest_t *manifest,
                              ota_digest_fn digest,
                              uint8_t out[OTA_MANIFEST_HASH_LEN]);

/**
 * @brief Verify the signature of a manifest.
 *
 * @param manifest Manifest to verify. May not be NULL.
 * @param verify   Verification callback. May not be NULL.
 * @return ESP_OK when the signature is valid, ESP_ERR_INVALID_STATE
 *         otherwise.
 */
esp_err_t ota_manifest_verify_signature(const ota_manifest_t *manifest,
                                         ota_signature_verify_fn verify);

/**
 * @brief Check that a manifest may be installed on this device.
 *
 * The product, the brand, the model, the hardware revision and the target
 * chip must all match exactly. This is the check that refuses a foreign
 * image *before* a single byte of it is downloaded.
 *
 * @param manifest Manifest to check. May not be NULL.
 * @param device   Identity of the device. May not be NULL.
 * @param reason   Optional destination for the rejection reason.
 * @return ESP_OK when the image is compatible.
 */
esp_err_t ota_manifest_check_compatible(const ota_manifest_t *manifest,
                                        const ota_device_identity_t *device,
                                        ota_reject_reason_t *reason);

/**
 * @brief Check that the channel of a manifest is admissible for a device.
 *
 * A device on channel `C` only accepts images whose channel is at least as
 * stable as `C`: a `dev` unit takes `dev`, `beta` and `production`, a
 * `beta` unit takes `beta` and `production`, and a `production` unit only
 * takes `production`. Unknown channels are refused on both sides, so a
 * typo can never widen the acceptance.
 *
 * @param manifest       Manifest to check. May not be NULL.
 * @param device_channel Channel of the device. May not be NULL.
 * @param reason         Optional destination for the rejection reason.
 * @return ESP_OK when the channel is admissible.
 */
esp_err_t ota_manifest_check_channel(const ota_manifest_t *manifest,
                                     const char *device_channel,
                                     ota_reject_reason_t *reason);

/**
 * @brief Check that a manifest does not downgrade the running firmware.
 *
 * @param manifest       Manifest to check. May not be NULL.
 * @param device_version Version currently running. May not be NULL.
 * @param reason         Optional destination for the rejection reason.
 * @return ESP_OK when the image is not older than the running firmware.
 */
esp_err_t ota_manifest_check_version(const ota_manifest_t *manifest,
                                     const char *device_version,
                                     ota_reject_reason_t *reason);

/**
 * @brief Compare a digest with the one declared by a manifest.
 *
 * @param manifest Manifest holding the expected digest. May not be NULL.
 * @param digest   Digest of the image. May not be NULL.
 * @return true when both digests are equal.
 */
bool ota_manifest_hash_matches(const ota_manifest_t *manifest,
                               const uint8_t digest[OTA_MANIFEST_HASH_LEN]);

/**
 * @brief Decode a hexadecimal digest.
 *
 * @param hex Hexadecimal string. May not be NULL.
 * @param out Destination digest. May not be NULL.
 * @return ESP_OK on success, ESP_ERR_INVALID_ARG for a NULL pointer or a
 *         malformed string, ESP_ERR_INVALID_SIZE for a wrong length.
 */
esp_err_t ota_hex_decode(const char *hex, uint8_t *out, size_t out_len);

/**
 * @brief Decode a base64 string.
 *
 * The standard alphabet is accepted, with or without padding.
 *
 * @param text      Base64 text. May not be NULL.
 * @param out       Destination buffer.
 * @param out_len   Size of @p out.
 * @param out_written Optional destination for the number of decoded bytes.
 * @return ESP_OK on success, ESP_ERR_INVALID_ARG for a malformed string,
 *         ESP_ERR_INVALID_SIZE when the buffer is too small.
 */
esp_err_t ota_base64_decode(const char *text, uint8_t *out, size_t out_len,
                            size_t *out_written);

/**
 * @brief Compare two dotted version strings.
 *
 * The components are compared numerically, so "1.10.0" is newer than
 * "1.9.0". A component that is not a number compares as zero.
 *
 * @param a First version. May not be NULL.
 * @param b Second version. May not be NULL.
 * @return A negative number when @p a is older, zero when both are equal
 *         and a positive number when @p a is newer.
 */
int ota_version_compare(const char *a, const char *b);

/**
 * @brief Return the stability rank of a channel name.
 *
 * "dev", "beta", "production" and "stable" (an alias of production) are
 * recognised.
 *
 * @param channel Channel name. May be NULL.
 * @return The rank, or OTA_CHANNEL_UNKNOWN.
 */
ota_channel_rank_t ota_channel_rank(const char *channel);

/**
 * @brief Return the stable name of a rejection reason.
 *
 * @param reason Reason to name.
 * @return A NUL-terminated string that never changes.
 */
const char *ota_reject_reason_name(ota_reject_reason_t reason);

#ifdef __cplusplus
}
#endif
