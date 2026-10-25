/**
 * @file ota_manifest.c
 * @brief Portable half of the OTA manifest layer.
 *
 * Everything in this file is plain C: no ESP-IDF call, no global state, no
 * allocation. That is what makes the interesting part of the OTA layer — the
 * rules that decide whether an image may be installed — testable on the host
 * (`tools/host-tests`) as well as on the device.
 *
 * The document parser is deliberately narrow. A release manifest is a flat
 * JSON object with a fixed set of fields, so the parser accepts exactly
 * that and refuses everything else: an unknown field, a duplicated field, a
 * nested value or a boolean is a malformed manifest. Being strict here is a
 * security decision, not a limitation — the signature only covers the
 * canonical form of the known fields, so a field this parser ignores could
 * never be authenticated.
 *
 * The ESP-IDF half (fetching, downloading, writing the partition) lives in
 * `ota_manager`.
 */

#include "ota_manifest.h"

#include <stdio.h>
#include <string.h>

/** The canonical form starts with this marker, so a payload can never be
 *  confused with the document it was derived from. */
#define OTA_MANIFEST_CANONICAL_HEADER "iiot-ota-manifest/v1"

/* Fields of the schema, as a bit mask. */
#define OTA_FIELD_SCHEMA          (1u << 0)
#define OTA_FIELD_PRODUCT         (1u << 1)
#define OTA_FIELD_BRAND           (1u << 2)
#define OTA_FIELD_MODEL           (1u << 3)
#define OTA_FIELD_REVISION        (1u << 4)
#define OTA_FIELD_TARGET          (1u << 5)
#define OTA_FIELD_VERSION         (1u << 6)
#define OTA_FIELD_CHANNEL         (1u << 7)
#define OTA_FIELD_SIZE            (1u << 8)
#define OTA_FIELD_SHA256          (1u << 9)
#define OTA_FIELD_URL             (1u << 10)
#define OTA_FIELD_SIGNATURE       (1u << 11)
#define OTA_FIELD_KEY_ID          (1u << 12)
#define OTA_FIELD_MANIFEST_SHA256 (1u << 13)

/** Every field of the schema except the optional manifest digest. */
#define OTA_FIELD_REQUIRED (OTA_FIELD_SCHEMA | OTA_FIELD_PRODUCT |          \
                            OTA_FIELD_BRAND | OTA_FIELD_MODEL |              \
                            OTA_FIELD_REVISION | OTA_FIELD_TARGET |          \
                            OTA_FIELD_VERSION | OTA_FIELD_CHANNEL |          \
                            OTA_FIELD_SIZE | OTA_FIELD_SHA256 |              \
                            OTA_FIELD_URL | OTA_FIELD_SIGNATURE |            \
                            OTA_FIELD_KEY_ID)

/** Length of the raw ECDSA P-256 signature: two 32-byte integers, r and s. */
#define OTA_SIGNATURE_RAW_LEN 64u

/**
 * @brief One field of a JSON document.
 */
typedef struct {
    const char *key;    /**< Field name, NUL-terminated. */
    size_t key_len;     /**< Length of @p key. */
    char *value;        /**< Decoded string value, or NULL for a number. */
    size_t value_len;   /**< Length of @p value. */
    bool is_string;     /**< true when the value is a string. */
    long long integer;  /**< Numeric value, valid when @p is_string is false. */
} ota_json_field_t;

/**
 * @brief Callback invoked for every field of a JSON object.
 */
typedef esp_err_t (*ota_json_field_cb)(const ota_json_field_t *field, void *ctx);

/**
 * @brief State of a manifest being parsed.
 */
typedef struct {
    ota_manifest_t *manifest; /**< Manifest being filled. */
    uint32_t seen;            /**< Bit mask of the fields already read. */
    ota_reject_reason_t reason; /**< Reason of the first rejection. */
} ota_parse_ctx_t;

static int ota_hex_digit(char c)
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

static int ota_base64_value(char c)
{
    if (c >= 'A' && c <= 'Z') {
        return c - 'A';
    }
    if (c >= 'a' && c <= 'z') {
        return c - 'a' + 26;
    }
    if (c >= '0' && c <= '9') {
        return c - '0' + 52;
    }
    if (c == '+') {
        return 62;
    }
    if (c == '/') {
        return 63;
    }
    return -1;
}

static bool ota_text_equal(const char *a, const char *b)
{
    if (a == NULL || b == NULL) {
        return false;
    }
    return strcmp(a, b) == 0;
}

static bool ota_has_line_break(const char *value, size_t value_len)
{
    for (size_t i = 0; i < value_len; i++) {
        if (value[i] == '\n' || value[i] == '\r') {
            return true;
        }
    }
    return false;
}

static esp_err_t ota_copy_string(char *dest, size_t dest_len,
                                 const char *value, size_t value_len)
{
    if (value_len + 1u > dest_len) {
        return ESP_ERR_INVALID_SIZE;
    }
    memcpy(dest, value, value_len);
    dest[value_len] = '\0';
    return ESP_OK;
}

/* ------------------------------------------------------------------ */
/* JSON                                                                */
/* ------------------------------------------------------------------ */

static const char *ota_json_skip_ws(const char *p, const char *end)
{
    while (p < end && (*p == ' ' || *p == '\t' || *p == '\n' || *p == '\r')) {
        p++;
    }
    return p;
}

/**
 * @brief Read a JSON string.
 *
 * The escape sequences of RFC 8259 are supported. A `\u` escape outside the
 * ASCII range is refused: every field of the schema is an identifier, a URL
 * or a hexadecimal digest, all of them ASCII, so accepting more would only
 * widen the surface for no gain.
 */
static esp_err_t ota_json_parse_string(const char **pp, const char *end,
                                       char *out, size_t out_len,
                                       size_t *written_len)
{
    const char *p = *pp;
    if (p >= end || *p != '"') {
        return ESP_ERR_INVALID_ARG;
    }
    p++;

    size_t written = 0u;
    while (p < end) {
        char c = *p++;

        if (c == '"') {
            if (written + 1u > out_len) {
                return ESP_ERR_INVALID_SIZE;
            }
            out[written] = '\0';
            *written_len = written;
            *pp = p;
            return ESP_OK;
        }

        if (c == '\\') {
            if (p >= end) {
                return ESP_ERR_INVALID_ARG;
            }
            const char escaped = *p++;
            switch (escaped) {
            case '"':  c = '"';  break;
            case '\\': c = '\\'; break;
            case '/':  c = '/';  break;
            case 'b':  c = '\b'; break;
            case 'f':  c = '\f'; break;
            case 'n':  c = '\n'; break;
            case 'r':  c = '\r'; break;
            case 't':  c = '\t'; break;
            case 'u': {
                if ((size_t)(end - p) < 4u) {
                    return ESP_ERR_INVALID_ARG;
                }
                uint16_t code = 0u;
                for (int i = 0; i < 4; i++) {
                    const int digit = ota_hex_digit(p[i]);
                    if (digit < 0) {
                        return ESP_ERR_INVALID_ARG;
                    }
                    code = (uint16_t)((code << 4) | (uint16_t)digit);
                }
                p += 4;
                if (code > 0x7Fu) {
                    return ESP_ERR_INVALID_ARG;
                }
                c = (char)code;
                break;
            }
            default:
                return ESP_ERR_INVALID_ARG;
            }
        }

        if (written + 1u > out_len) {
            return ESP_ERR_INVALID_SIZE;
        }
        out[written++] = c;
    }

    return ESP_ERR_INVALID_ARG;
}

/**
 * @brief Read a JSON integer.
 *
 * A fractional or exponential part is refused: the only numeric field of the
 * schema is the size of the image, and it is a whole number of bytes.
 */
static esp_err_t ota_json_parse_integer(const char **pp, const char *end,
                                         long long *out)
{
    const char *p = *pp;
    bool negative = false;

    if (p < end && *p == '-') {
        negative = true;
        p++;
    }
    if (p >= end || *p < '0' || *p > '9') {
        return ESP_ERR_INVALID_ARG;
    }

    long long value = 0;
    while (p < end && *p >= '0' && *p <= '9') {
        if (value > (2147483647LL - 9LL) / 10LL) {
            return ESP_ERR_INVALID_SIZE;
        }
        value = value * 10LL + (long long)(*p - '0');
        p++;
    }

    if (p < end && (*p == '.' || *p == 'e' || *p == 'E')) {
        return ESP_ERR_INVALID_ARG;
    }

    *out = negative ? -value : value;
    *pp = p;
    return ESP_OK;
}

/**
 * @brief Walk a flat JSON object.
 *
 * Only string and integer values are accepted. Anything else — a nested
 * object, an array, a boolean or null — makes the document invalid.
 */
static esp_err_t ota_json_parse_object(const char *json, size_t json_len,
                                       char *scratch, size_t scratch_len,
                                       ota_json_field_cb cb, void *ctx)
{
    const char *p = json;
    const char *end = json + json_len;

    p = ota_json_skip_ws(p, end);
    if (p >= end || *p != '{') {
        return ESP_ERR_INVALID_ARG;
    }
    p++;
    p = ota_json_skip_ws(p, end);
    if (p < end && *p == '}') {
        return ESP_OK;
    }

    while (true) {
        char key[OTA_MANIFEST_KEY_ID_MAX_LEN] = {0};
        size_t key_len = 0u;

        p = ota_json_skip_ws(p, end);
        esp_err_t err = ota_json_parse_string(&p, end, key, sizeof(key), &key_len);
        if (err != ESP_OK) {
            return err;
        }

        p = ota_json_skip_ws(p, end);
        if (p >= end || *p != ':') {
            return ESP_ERR_INVALID_ARG;
        }
        p++;
        p = ota_json_skip_ws(p, end);
        if (p >= end) {
            return ESP_ERR_INVALID_ARG;
        }

        ota_json_field_t field;
        memset(&field, 0, sizeof(field));
        field.key = key;
        field.key_len = key_len;

        if (*p == '"') {
            size_t value_len = 0u;
            err = ota_json_parse_string(&p, end, scratch, scratch_len, &value_len);
            if (err != ESP_OK) {
                return err;
            }
            field.is_string = true;
            field.value = scratch;
            field.value_len = value_len;
        } else if ((*p >= '0' && *p <= '9') || *p == '-') {
            long long integer = 0;
            err = ota_json_parse_integer(&p, end, &integer);
            if (err != ESP_OK) {
                return err;
            }
            field.integer = integer;
        } else {
            return ESP_ERR_INVALID_ARG;
        }

        err = cb(&field, ctx);
        if (err != ESP_OK) {
            return err;
        }

        p = ota_json_skip_ws(p, end);
        if (p >= end) {
            return ESP_ERR_INVALID_ARG;
        }
        if (*p == ',') {
            p++;
            continue;
        }
        if (*p == '}') {
            p++;
            break;
        }
        return ESP_ERR_INVALID_ARG;
    }

    p = ota_json_skip_ws(p, end);
    return (p == end) ? ESP_OK : ESP_ERR_INVALID_ARG;
}

/* ------------------------------------------------------------------ */
/* Manifest                                                            */
/* ------------------------------------------------------------------ */

/**
 * @brief Copy a string field into the manifest and check its shape.
 */
static esp_err_t ota_manifest_take_string(ota_parse_ctx_t *ctx,
                                          char *dest, size_t dest_len,
                                          const ota_json_field_t *field)
{
    if (!field->is_string) {
        ctx->reason = OTA_REJECT_MALFORMED;
        return ESP_ERR_INVALID_ARG;
    }
    if (ota_has_line_break(field->value, field->value_len)) {
        ctx->reason = OTA_REJECT_MALFORMED;
        return ESP_ERR_INVALID_ARG;
    }
    const esp_err_t err = ota_copy_string(dest, dest_len, field->value,
                                           field->value_len);
    if (err != ESP_OK) {
        ctx->reason = OTA_REJECT_MALFORMED;
    }
    return err;
}

/**
 * @brief Copy the SHA-256 field, normalising it to lowercase hexadecimal.
 */
static esp_err_t ota_manifest_take_digest(ota_parse_ctx_t *ctx,
                                          char *dest,
                                          const ota_json_field_t *field)
{
    if (!field->is_string) {
        ctx->reason = OTA_REJECT_MALFORMED;
        return ESP_ERR_INVALID_ARG;
    }
    if (field->value_len != OTA_MANIFEST_HASH_HEX_LEN) {
        ctx->reason = OTA_REJECT_MALFORMED;
        return ESP_ERR_INVALID_SIZE;
    }

    for (size_t i = 0; i < field->value_len; i++) {
        const char c = field->value[i];
        if (ota_hex_digit(c) < 0) {
            ctx->reason = OTA_REJECT_MALFORMED;
            return ESP_ERR_INVALID_ARG;
        }
        dest[i] = (char)((c >= 'A' && c <= 'F') ? (c - 'A' + 'a') : c);
    }
    dest[OTA_MANIFEST_HASH_HEX_LEN] = '\0';
    return ESP_OK;
}

static esp_err_t ota_manifest_field(const ota_json_field_t *field, void *ctx)
{
    ota_parse_ctx_t *parse = ctx;
    ota_manifest_t *manifest = parse->manifest;

    uint32_t bit = 0u;
    if (strcmp(field->key, "schema") == 0) {
        bit = OTA_FIELD_SCHEMA;
    } else if (strcmp(field->key, "product") == 0) {
        bit = OTA_FIELD_PRODUCT;
    } else if (strcmp(field->key, "brand") == 0) {
        bit = OTA_FIELD_BRAND;
    } else if (strcmp(field->key, "model") == 0) {
        bit = OTA_FIELD_MODEL;
    } else if (strcmp(field->key, "revision") == 0) {
        bit = OTA_FIELD_REVISION;
    } else if (strcmp(field->key, "target") == 0) {
        bit = OTA_FIELD_TARGET;
    } else if (strcmp(field->key, "version") == 0) {
        bit = OTA_FIELD_VERSION;
    } else if (strcmp(field->key, "channel") == 0) {
        bit = OTA_FIELD_CHANNEL;
    } else if (strcmp(field->key, "size") == 0) {
        bit = OTA_FIELD_SIZE;
    } else if (strcmp(field->key, "sha256") == 0) {
        bit = OTA_FIELD_SHA256;
    } else if (strcmp(field->key, "url") == 0) {
        bit = OTA_FIELD_URL;
    } else if (strcmp(field->key, "signature") == 0) {
        bit = OTA_FIELD_SIGNATURE;
    } else if (strcmp(field->key, "key_id") == 0) {
        bit = OTA_FIELD_KEY_ID;
    } else if (strcmp(field->key, "manifest_sha256") == 0) {
        bit = OTA_FIELD_MANIFEST_SHA256;
    } else {
        /* An unknown field cannot be authenticated: the signature only
         * covers the canonical form of the known ones. */
        parse->reason = OTA_REJECT_MALFORMED;
        return ESP_ERR_INVALID_ARG;
    }

    if ((parse->seen & bit) != 0u) {
        parse->reason = OTA_REJECT_MALFORMED;
        return ESP_ERR_INVALID_ARG;
    }
    parse->seen |= bit;

    switch (bit) {
    case OTA_FIELD_SCHEMA:
        if (field->is_string) {
            parse->reason = OTA_REJECT_MALFORMED;
            return ESP_ERR_INVALID_ARG;
        }
        if (field->integer != (long long)OTA_MANIFEST_SCHEMA) {
            parse->reason = OTA_REJECT_SCHEMA;
            return ESP_ERR_INVALID_VERSION;
        }
        manifest->schema = (uint32_t)field->integer;
        return ESP_OK;

    case OTA_FIELD_SIZE:
        if (field->is_string) {
            parse->reason = OTA_REJECT_MALFORMED;
            return ESP_ERR_INVALID_ARG;
        }
        if (field->integer <= 0 || field->integer > 0xFFFFFFFFLL) {
            parse->reason = OTA_REJECT_IMAGE_SIZE;
            return ESP_ERR_INVALID_SIZE;
        }
        manifest->size = (uint32_t)field->integer;
        return ESP_OK;

    case OTA_FIELD_PRODUCT:
        return ota_manifest_take_string(parse, manifest->product,
                                         sizeof(manifest->product), field);
    case OTA_FIELD_BRAND:
        return ota_manifest_take_string(parse, manifest->brand,
                                         sizeof(manifest->brand), field);
    case OTA_FIELD_MODEL:
        return ota_manifest_take_string(parse, manifest->model,
                                         sizeof(manifest->model), field);
    case OTA_FIELD_REVISION:
        return ota_manifest_take_string(parse, manifest->revision,
                                         sizeof(manifest->revision), field);
    case OTA_FIELD_TARGET:
        return ota_manifest_take_string(parse, manifest->target,
                                         sizeof(manifest->target), field);
    case OTA_FIELD_VERSION:
        return ota_manifest_take_string(parse, manifest->version,
                                         sizeof(manifest->version), field);
    case OTA_FIELD_CHANNEL:
        return ota_manifest_take_string(parse, manifest->channel,
                                         sizeof(manifest->channel), field);
    case OTA_FIELD_URL:
        return ota_manifest_take_string(parse, manifest->url,
                                         sizeof(manifest->url), field);
    case OTA_FIELD_SIGNATURE:
        return ota_manifest_take_string(parse, manifest->signature,
                                         sizeof(manifest->signature), field);
    case OTA_FIELD_KEY_ID:
        return ota_manifest_take_string(parse, manifest->key_id,
                                         sizeof(manifest->key_id), field);
    case OTA_FIELD_SHA256:
        return ota_manifest_take_digest(parse, manifest->sha256, field);
    case OTA_FIELD_MANIFEST_SHA256:
        return ota_manifest_take_digest(parse, manifest->manifest_sha256, field);

    default:
        parse->reason = OTA_REJECT_MALFORMED;
        return ESP_ERR_INVALID_ARG;
    }
}

esp_err_t ota_manifest_parse(const char *json, size_t json_len,
                             ota_manifest_t *out, ota_reject_reason_t *reason)
{
    if (json == NULL || out == NULL) {
        return ESP_ERR_INVALID_ARG;
    }

    memset(out, 0, sizeof(*out));

    ota_parse_ctx_t ctx = {
        .manifest = out,
        .seen = 0u,
        .reason = OTA_REJECT_MALFORMED,
    };

    char scratch[OTA_MANIFEST_URL_MAX_LEN] = {0};
    const esp_err_t err = ota_json_parse_object(json, json_len, scratch,
                                                 sizeof(scratch),
                                                 ota_manifest_field, &ctx);
    if (err != ESP_OK) {
        if (reason != NULL) {
            *reason = ctx.reason;
        }
        return err;
    }

    if ((ctx.seen & OTA_FIELD_REQUIRED) != OTA_FIELD_REQUIRED) {
        if (reason != NULL) {
            *reason = OTA_REJECT_MALFORMED;
        }
        return ESP_ERR_INVALID_ARG;
    }

    /* The image is only ever fetched over TLS: a manifest that points to a
     * plain HTTP URL is refused before anything is downloaded. */
    if (strncmp(out->url, "https://", 8) != 0) {
        if (reason != NULL) {
            *reason = OTA_REJECT_MALFORMED;
        }
        return ESP_ERR_INVALID_ARG;
    }

    if (reason != NULL) {
        *reason = OTA_REJECT_NONE;
    }
    return ESP_OK;
}

/* ------------------------------------------------------------------ */
/* Canonical form                                                      */
/* ------------------------------------------------------------------ */

static int ota_manifest_format(const ota_manifest_t *manifest, char *out,
                               size_t out_len)
{
    return snprintf(out, out_len,
                    OTA_MANIFEST_CANONICAL_HEADER "\n"
                    "schema=%u\n"
                    "product=%s\n"
                    "brand=%s\n"
                    "model=%s\n"
                    "revision=%s\n"
                    "target=%s\n"
                    "version=%s\n"
                    "channel=%s\n"
                    "size=%u\n"
                    "sha256=%s\n"
                    "url=%s\n"
                    "key_id=%s\n",
                    (unsigned int)manifest->schema,
                    manifest->product,
                    manifest->brand,
                    manifest->model,
                    manifest->revision,
                    manifest->target,
                    manifest->version,
                    manifest->channel,
                    (unsigned int)manifest->size,
                    manifest->sha256,
                    manifest->url,
                    manifest->key_id);
}

esp_err_t ota_manifest_canonical_payload(const ota_manifest_t *manifest,
                                          char *out, size_t out_len)
{
    if (manifest == NULL || out == NULL) {
        return ESP_ERR_INVALID_ARG;
    }

    const int written = ota_manifest_format(manifest, out, out_len);
    if (written < 0) {
        return ESP_ERR_INVALID_ARG;
    }
    if ((size_t)written >= out_len) {
        return ESP_ERR_INVALID_SIZE;
    }
    return ESP_OK;
}

size_t ota_manifest_canonical_payload_size(const ota_manifest_t *manifest)
{
    if (manifest == NULL) {
        return 0u;
    }

    const int needed = ota_manifest_format(manifest, NULL, 0u);
    if (needed < 0) {
        return 0u;
    }
    return (size_t)needed + 1u;
}

esp_err_t ota_manifest_digest(const ota_manifest_t *manifest,
                              ota_digest_fn digest,
                              uint8_t out[OTA_MANIFEST_HASH_LEN])
{
    if (manifest == NULL || digest == NULL || out == NULL) {
        return ESP_ERR_INVALID_ARG;
    }

    char payload[OTA_MANIFEST_PAYLOAD_MAX_LEN] = {0};
    const esp_err_t err = ota_manifest_canonical_payload(manifest, payload,
                                                          sizeof(payload));
    if (err != ESP_OK) {
        return err;
    }

    return digest(payload, strlen(payload), out);
}

esp_err_t ota_manifest_verify_signature(const ota_manifest_t *manifest,
                                         ota_signature_verify_fn verify)
{
    if (manifest == NULL || verify == NULL) {
        return ESP_ERR_INVALID_ARG;
    }

    uint8_t signature[OTA_SIGNATURE_RAW_LEN] = {0};
    size_t signature_len = 0u;
    const esp_err_t err = ota_base64_decode(manifest->signature, signature,
                                            sizeof(signature), &signature_len);
    if (err != ESP_OK) {
        return ESP_ERR_INVALID_STATE;
    }
    if (signature_len != OTA_SIGNATURE_RAW_LEN) {
        return ESP_ERR_INVALID_STATE;
    }

    char payload[OTA_MANIFEST_PAYLOAD_MAX_LEN] = {0};
    const esp_err_t payload_err = ota_manifest_canonical_payload(manifest,
                                                                  payload,
                                                                  sizeof(payload));
    if (payload_err != ESP_OK) {
        return payload_err;
    }

    return verify(signature, signature_len, (const uint8_t *)payload,
                  strlen(payload));
}

/* ------------------------------------------------------------------ */
/* Rules                                                               */
/* ------------------------------------------------------------------ */

esp_err_t ota_manifest_check_compatible(const ota_manifest_t *manifest,
                                        const ota_device_identity_t *device,
                                        ota_reject_reason_t *reason)
{
    if (manifest == NULL || device == NULL) {
        return ESP_ERR_INVALID_ARG;
    }

    ota_reject_reason_t local = OTA_REJECT_NONE;
    if (!ota_text_equal(manifest->product, device->product)) {
        local = OTA_REJECT_PRODUCT;
    } else if (!ota_text_equal(manifest->brand, device->brand)) {
        local = OTA_REJECT_BRAND;
    } else if (!ota_text_equal(manifest->model, device->model)) {
        local = OTA_REJECT_MODEL;
    } else if (!ota_text_equal(manifest->revision, device->revision)) {
        local = OTA_REJECT_REVISION;
    } else if (!ota_text_equal(manifest->target, device->target)) {
        local = OTA_REJECT_TARGET;
    }

    if (reason != NULL) {
        *reason = local;
    }
    return (local == OTA_REJECT_NONE) ? ESP_OK : ESP_ERR_INVALID_STATE;
}

esp_err_t ota_manifest_check_channel(const ota_manifest_t *manifest,
                                     const char *device_channel,
                                     ota_reject_reason_t *reason)
{
    if (manifest == NULL || device_channel == NULL) {
        return ESP_ERR_INVALID_ARG;
    }

    const ota_channel_rank_t device = ota_channel_rank(device_channel);
    const ota_channel_rank_t image = ota_channel_rank(manifest->channel);

    /* An unknown channel on either side is a refusal, never a widening:
     * a typo in the build configuration must not open the door. */
    const bool admissible = (device != OTA_CHANNEL_UNKNOWN) &&
                            (image != OTA_CHANNEL_UNKNOWN) &&
                            (image >= device);

    if (reason != NULL) {
        *reason = admissible ? OTA_REJECT_NONE : OTA_REJECT_CHANNEL;
    }
    return admissible ? ESP_OK : ESP_ERR_INVALID_STATE;
}

esp_err_t ota_manifest_check_version(const ota_manifest_t *manifest,
                                     const char *device_version,
                                     ota_reject_reason_t *reason)
{
    if (manifest == NULL || device_version == NULL) {
        return ESP_ERR_INVALID_ARG;
    }

    const bool admissible = ota_version_compare(manifest->version,
                                                device_version) >= 0;
    if (reason != NULL) {
        *reason = admissible ? OTA_REJECT_NONE : OTA_REJECT_DOWNGRADE;
    }
    return admissible ? ESP_OK : ESP_ERR_INVALID_STATE;
}

bool ota_manifest_hash_matches(const ota_manifest_t *manifest,
                               const uint8_t digest[OTA_MANIFEST_HASH_LEN])
{
    if (manifest == NULL || digest == NULL) {
        return false;
    }

    uint8_t expected[OTA_MANIFEST_HASH_LEN] = {0};
    if (ota_hex_decode(manifest->sha256, expected, sizeof(expected)) != ESP_OK) {
        return false;
    }
    return memcmp(expected, digest, sizeof(expected)) == 0;
}

/* ------------------------------------------------------------------ */
/* Helpers                                                             */
/* ------------------------------------------------------------------ */

esp_err_t ota_hex_decode(const char *hex, uint8_t *out, size_t out_len)
{
    if (hex == NULL || out == NULL) {
        return ESP_ERR_INVALID_ARG;
    }

    const size_t len = strnlen(hex, (out_len * 2u) + 1u);
    if (len != out_len * 2u) {
        return ESP_ERR_INVALID_SIZE;
    }

    for (size_t i = 0; i < out_len; i++) {
        const int high = ota_hex_digit(hex[i * 2u]);
        const int low = ota_hex_digit(hex[(i * 2u) + 1u]);
        if (high < 0 || low < 0) {
            return ESP_ERR_INVALID_ARG;
        }
        out[i] = (uint8_t)((high << 4) | low);
    }
    return ESP_OK;
}

esp_err_t ota_base64_decode(const char *text, uint8_t *out, size_t out_len,
                            size_t *out_written)
{
    if (text == NULL || out == NULL) {
        return ESP_ERR_INVALID_ARG;
    }

    size_t written = 0u;
    uint32_t accumulator = 0u;
    int bits = 0;
    bool padding_seen = false;

    for (const char *p = text; *p != '\0'; p++) {
        const char c = *p;
        if (c == '\n' || c == '\r' || c == ' ' || c == '\t') {
            continue;
        }
        if (c == '=') {
            padding_seen = true;
            continue;
        }
        if (padding_seen) {
            /* Data after the padding is malformed. */
            return ESP_ERR_INVALID_ARG;
        }

        const int value = ota_base64_value(c);
        if (value < 0) {
            return ESP_ERR_INVALID_ARG;
        }

        accumulator = (accumulator << 6) | (uint32_t)value;
        bits += 6;
        if (bits >= 8) {
            bits -= 8;
            if (written >= out_len) {
                return ESP_ERR_INVALID_SIZE;
            }
            out[written++] = (uint8_t)((accumulator >> bits) & 0xFFu);
            accumulator &= (bits == 0) ? 0u : ((1u << bits) - 1u);
        }
    }

    if (out_written != NULL) {
        *out_written = written;
    }
    return ESP_OK;
}

/**
 * @brief Read the next component of a dotted version.
 */
static long ota_version_component(const char **pp)
{
    const char *p = *pp;
    long value = 0;
    bool digits = false;

    while (*p >= '0' && *p <= '9') {
        value = (value * 10L) + (long)(*p - '0');
        p++;
        digits = true;
    }
    if (*p == '.') {
        p++;
    }

    *pp = p;
    return digits ? value : 0L;
}

int ota_version_compare(const char *a, const char *b)
{
    if (a == NULL && b == NULL) {
        return 0;
    }
    if (a == NULL) {
        return -1;
    }
    if (b == NULL) {
        return 1;
    }

    const char *pa = a;
    const char *pb = b;
    while (*pa != '\0' || *pb != '\0') {
        const long va = ota_version_component(&pa);
        const long vb = ota_version_component(&pb);
        if (va != vb) {
            return (va < vb) ? -1 : 1;
        }
    }
    return 0;
}

ota_channel_rank_t ota_channel_rank(const char *channel)
{
    if (channel == NULL) {
        return OTA_CHANNEL_UNKNOWN;
    }
    if (strcmp(channel, "dev") == 0) {
        return OTA_CHANNEL_DEV;
    }
    if (strcmp(channel, "beta") == 0) {
        return OTA_CHANNEL_BETA;
    }
    /* "stable" is the name the reference profile ships with; it is the same
     * environment as "production". */
    if (strcmp(channel, "production") == 0 || strcmp(channel, "stable") == 0) {
        return OTA_CHANNEL_PRODUCTION;
    }
    return OTA_CHANNEL_UNKNOWN;
}

const char *ota_reject_reason_name(ota_reject_reason_t reason)
{
    switch (reason) {
    case OTA_REJECT_NONE:          return "none";
    case OTA_REJECT_MALFORMED:     return "malformed-manifest";
    case OTA_REJECT_SCHEMA:        return "unsupported-schema";
    case OTA_REJECT_SIGNATURE:     return "bad-signature";
    case OTA_REJECT_MANIFEST_HASH: return "bad-manifest-digest";
    case OTA_REJECT_PRODUCT:       return "product-mismatch";
    case OTA_REJECT_BRAND:         return "brand-mismatch";
    case OTA_REJECT_MODEL:         return "model-mismatch";
    case OTA_REJECT_REVISION:      return "revision-mismatch";
    case OTA_REJECT_TARGET:        return "target-mismatch";
    case OTA_REJECT_CHANNEL:       return "channel-not-admissible";
    case OTA_REJECT_DOWNGRADE:     return "downgrade-refused";
    case OTA_REJECT_IMAGE_HASH:    return "bad-image-digest";
    case OTA_REJECT_IMAGE_SIZE:    return "bad-image-size";
    default:                       return "unknown";
    }
}
