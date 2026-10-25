#include "unity.h"

#include "ota_manifest.h"

#include <stdio.h>
#include <string.h>

/* The digest of the image, as it appears in the manifest. It is a macro so
 * the documents below can be built by concatenation. */
#define K_DIGEST_HEX "2f2d5b61c3a9e4f0d8b7a6c5e4d3c2b1a09f8e7d6c5b4a39281706f5e4d3c2b1"

/* A complete, valid manifest. The document and the expected canonical
 * payload are written as two independent literals on purpose: if the
 * canonical form ever drifts from what the release tooling signs, this test
 * fails instead of the two sides disagreeing in the field. */
static const char *kManifestJson =
    "{\"schema\":1,"
    "\"product\":\"iiot-kit-gateway\","
    "\"brand\":\"IIoT-Kit\","
    "\"model\":\"GW-S3-1\","
    "\"revision\":\"rev-a\","
    "\"target\":\"esp32s3\","
    "\"version\":\"1.1.0\","
    "\"channel\":\"production\","
    "\"size\":1011344,"
    "\"sha256\":\"" K_DIGEST_HEX "\","
    "\"url\":\"https://example.org/firmware/gateway.bin\","
    "\"signature\":\"AAECAwQFBgcICQoLDA0ODxAREhMUFRYXGBkaGxwdHh8gISIjJCUmJygpKissLS4vMDEyMzQ1Njc4OTo7PD0+Pw==\","
    "\"key_id\":\"ota-2026\"}";

static const char *kCanonicalPayload =
    "iiot-ota-manifest/v1\n"
    "schema=1\n"
    "product=iiot-kit-gateway\n"
    "brand=IIoT-Kit\n"
    "model=GW-S3-1\n"
    "revision=rev-a\n"
    "target=esp32s3\n"
    "version=1.1.0\n"
    "channel=production\n"
    "size=1011344\n"
    "sha256=" K_DIGEST_HEX "\n"
    "url=https://example.org/firmware/gateway.bin\n"
    "key_id=ota-2026\n";

static const char *kDigestHex = K_DIGEST_HEX;

/* Scratch shared with the verification double below. */
static char g_seen_payload[OTA_MANIFEST_PAYLOAD_MAX_LEN];
static size_t g_seen_signature_len;
static uint8_t g_seen_signature[64];
static bool g_accept_signature;

static esp_err_t verify_double(const uint8_t *signature, size_t signature_len,
                                const uint8_t *data, size_t data_len)
{
    if (signature_len <= sizeof(g_seen_signature)) {
        memcpy(g_seen_signature, signature, signature_len);
    }
    g_seen_signature_len = signature_len;
    if (data_len < sizeof(g_seen_payload)) {
        memcpy(g_seen_payload, data, data_len);
        g_seen_payload[data_len] = '\0';
    }
    return g_accept_signature ? ESP_OK : ESP_ERR_INVALID_STATE;
}

static ota_manifest_t parse_ok(ota_reject_reason_t *reason)
{
    ota_manifest_t manifest;
    memset(&manifest, 0, sizeof(manifest));

    const esp_err_t err = ota_manifest_parse(kManifestJson,
                                             strlen(kManifestJson),
                                             &manifest, reason);
    TEST_ASSERT_EQUAL(ESP_OK, err);
    return manifest;
}

static void identity_of(ota_device_identity_t *device)
{
    device->product = "iiot-kit-gateway";
    device->brand = "IIoT-Kit";
    device->model = "GW-S3-1";
    device->revision = "rev-a";
    device->target = "esp32s3";
    device->version = "1.0.0";
    device->channel = "production";
}

TEST_CASE("Manifest parsing reads every field of the schema", "[ota_manifest]")
{
    ota_reject_reason_t reason = OTA_REJECT_MALFORMED;
    const ota_manifest_t manifest = parse_ok(&reason);

    TEST_ASSERT_EQUAL(OTA_REJECT_NONE, reason);
    TEST_ASSERT_EQUAL_UINT32(1u, manifest.schema);
    TEST_ASSERT_EQUAL_STRING("iiot-kit-gateway", manifest.product);
    TEST_ASSERT_EQUAL_STRING("IIoT-Kit", manifest.brand);
    TEST_ASSERT_EQUAL_STRING("GW-S3-1", manifest.model);
    TEST_ASSERT_EQUAL_STRING("rev-a", manifest.revision);
    TEST_ASSERT_EQUAL_STRING("esp32s3", manifest.target);
    TEST_ASSERT_EQUAL_STRING("1.1.0", manifest.version);
    TEST_ASSERT_EQUAL_STRING("production", manifest.channel);
    TEST_ASSERT_EQUAL_UINT32(1011344u, manifest.size);
    TEST_ASSERT_EQUAL_STRING(kDigestHex, manifest.sha256);
    TEST_ASSERT_EQUAL_STRING("https://example.org/firmware/gateway.bin",
                             manifest.url);
    TEST_ASSERT_EQUAL_STRING("ota-2026", manifest.key_id);
}

TEST_CASE("Manifest parsing refuses a document it cannot authenticate",
          "[ota_manifest]")
{
    ota_manifest_t manifest;
    ota_reject_reason_t reason = OTA_REJECT_NONE;

    /* An unknown field is not covered by the signature. */
    const char *unknown =
        "{\"schema\":1,\"product\":\"p\",\"brand\":\"b\",\"model\":\"m\","
        "\"revision\":\"r\",\"target\":\"esp32s3\",\"version\":\"1.0.0\","
        "\"channel\":\"production\",\"size\":10,\"sha256\":\"" K_DIGEST_HEX "\","
        "\"url\":\"https://example.org/a.bin\",\"signature\":\"AA==\","
        "\"key_id\":\"k\",\"extra\":true}";
    TEST_ASSERT_NOT_EQUAL(ESP_OK,
                          ota_manifest_parse(unknown, strlen(unknown),
                                             &manifest, &reason));
    TEST_ASSERT_EQUAL(OTA_REJECT_MALFORMED, reason);

    /* A duplicated field is ambiguous. */
    const char *duplicate =
        "{\"schema\":1,\"schema\":1,\"product\":\"p\",\"brand\":\"b\","
        "\"model\":\"m\",\"revision\":\"r\",\"target\":\"esp32s3\","
        "\"version\":\"1.0.0\",\"channel\":\"production\",\"size\":10,"
        "\"sha256\":\"" K_DIGEST_HEX "\",\"url\":\"https://example.org/a.bin\","
        "\"signature\":\"AA==\",\"key_id\":\"k\"}";
    TEST_ASSERT_NOT_EQUAL(ESP_OK,
                          ota_manifest_parse(duplicate, strlen(duplicate),
                                             &manifest, &reason));
    TEST_ASSERT_EQUAL(OTA_REJECT_MALFORMED, reason);

    /* A missing field leaves the manifest incomplete. */
    const char *missing =
        "{\"schema\":1,\"product\":\"p\",\"brand\":\"b\",\"model\":\"m\","
        "\"revision\":\"r\",\"target\":\"esp32s3\",\"version\":\"1.0.0\","
        "\"channel\":\"production\",\"size\":10,\"sha256\":\"" K_DIGEST_HEX "\","
        "\"url\":\"https://example.org/a.bin\",\"key_id\":\"k\"}";
    TEST_ASSERT_NOT_EQUAL(ESP_OK,
                          ota_manifest_parse(missing, strlen(missing),
                                             &manifest, &reason));
    TEST_ASSERT_EQUAL(OTA_REJECT_MALFORMED, reason);
}

TEST_CASE("Manifest parsing refuses an unsupported schema", "[ota_manifest]")
{
    ota_manifest_t manifest;
    ota_reject_reason_t reason = OTA_REJECT_NONE;

    const char *future =
        "{\"schema\":2,\"product\":\"p\",\"brand\":\"b\",\"model\":\"m\","
        "\"revision\":\"r\",\"target\":\"esp32s3\",\"version\":\"1.0.0\","
        "\"channel\":\"production\",\"size\":10,\"sha256\":\"" K_DIGEST_HEX "\","
        "\"url\":\"https://example.org/a.bin\",\"signature\":\"AA==\","
        "\"key_id\":\"k\"}";
    TEST_ASSERT_NOT_EQUAL(ESP_OK,
                          ota_manifest_parse(future, strlen(future),
                                             &manifest, &reason));
    TEST_ASSERT_EQUAL(OTA_REJECT_SCHEMA, reason);
}

TEST_CASE("Manifest parsing refuses a plain HTTP image", "[ota_manifest]")
{
    ota_manifest_t manifest;
    ota_reject_reason_t reason = OTA_REJECT_NONE;

    const char *insecure =
        "{\"schema\":1,\"product\":\"p\",\"brand\":\"b\",\"model\":\"m\","
        "\"revision\":\"r\",\"target\":\"esp32s3\",\"version\":\"1.0.0\","
        "\"channel\":\"production\",\"size\":10,\"sha256\":\"" K_DIGEST_HEX "\","
        "\"url\":\"http://example.org/a.bin\",\"signature\":\"AA==\","
        "\"key_id\":\"k\"}";
    TEST_ASSERT_NOT_EQUAL(ESP_OK,
                          ota_manifest_parse(insecure, strlen(insecure),
                                             &manifest, &reason));
    TEST_ASSERT_EQUAL(OTA_REJECT_MALFORMED, reason);
}

TEST_CASE("The canonical payload is the exact bytes that get signed",
          "[ota_manifest]")
{
    ota_reject_reason_t reason = OTA_REJECT_NONE;
    const ota_manifest_t manifest = parse_ok(&reason);

    char payload[OTA_MANIFEST_PAYLOAD_MAX_LEN] = {0};
    TEST_ASSERT_EQUAL(ESP_OK,
                      ota_manifest_canonical_payload(&manifest, payload,
                                                    sizeof(payload)));
    TEST_ASSERT_EQUAL_STRING(kCanonicalPayload, payload);
    TEST_ASSERT_EQUAL_UINT32(strlen(kCanonicalPayload) + 1u,
                             ota_manifest_canonical_payload_size(&manifest));
}

TEST_CASE("A foreign image is refused before it is downloaded", "[ota_manifest]")
{
    ota_reject_reason_t reason = OTA_REJECT_NONE;
    const ota_manifest_t manifest = parse_ok(&reason);

    ota_device_identity_t device;
    identity_of(&device);

    TEST_ASSERT_EQUAL(ESP_OK,
                      ota_manifest_check_compatible(&manifest, &device, &reason));
    TEST_ASSERT_EQUAL(OTA_REJECT_NONE, reason);

    device.brand = "Acme Industrial";
    TEST_ASSERT_NOT_EQUAL(ESP_OK,
                          ota_manifest_check_compatible(&manifest, &device,
                                                       &reason));
    TEST_ASSERT_EQUAL(OTA_REJECT_BRAND, reason);

    device.brand = "IIoT-Kit";
    device.model = "AX-200";
    TEST_ASSERT_NOT_EQUAL(ESP_OK,
                          ota_manifest_check_compatible(&manifest, &device,
                                                       &reason));
    TEST_ASSERT_EQUAL(OTA_REJECT_MODEL, reason);

    device.model = "GW-S3-1";
    device.revision = "rev-b";
    TEST_ASSERT_NOT_EQUAL(ESP_OK,
                          ota_manifest_check_compatible(&manifest, &device,
                                                       &reason));
    TEST_ASSERT_EQUAL(OTA_REJECT_REVISION, reason);

    device.revision = "rev-a";
    device.product = "other-gateway";
    TEST_ASSERT_NOT_EQUAL(ESP_OK,
                          ota_manifest_check_compatible(&manifest, &device,
                                                       &reason));
    TEST_ASSERT_EQUAL(OTA_REJECT_PRODUCT, reason);

    device.product = "iiot-kit-gateway";
    device.target = "esp32c3";
    TEST_ASSERT_NOT_EQUAL(ESP_OK,
                          ota_manifest_check_compatible(&manifest, &device,
                                                       &reason));
    TEST_ASSERT_EQUAL(OTA_REJECT_TARGET, reason);
}

TEST_CASE("Release channels never promote a unit by accident", "[ota_manifest]")
{
    ota_reject_reason_t reason = OTA_REJECT_NONE;
    const ota_manifest_t manifest = parse_ok(&reason);

    /* The manifest carries the "production" channel. */
    TEST_ASSERT_EQUAL(OTA_CHANNEL_PRODUCTION, ota_channel_rank(manifest.channel));

    /* A production unit takes production images and nothing else. */
    TEST_ASSERT_EQUAL(ESP_OK,
                      ota_manifest_check_channel(&manifest, "production",
                                                 &reason));
    TEST_ASSERT_EQUAL(OTA_REJECT_NONE, reason);
    TEST_ASSERT_EQUAL(ESP_OK,
                      ota_manifest_check_channel(&manifest, "stable", &reason));

    /* A beta unit would take a production image. */
    TEST_ASSERT_EQUAL(ESP_OK,
                      ota_manifest_check_channel(&manifest, "beta", &reason));

    /* But a production unit never takes a beta image. */
    ota_manifest_t beta = manifest;
    strcpy(beta.channel, "beta");
    TEST_ASSERT_NOT_EQUAL(ESP_OK,
                          ota_manifest_check_channel(&beta, "production",
                                                     &reason));
    TEST_ASSERT_EQUAL(OTA_REJECT_CHANNEL, reason);

    /* Nor a development one. */
    ota_manifest_t dev = manifest;
    strcpy(dev.channel, "dev");
    TEST_ASSERT_NOT_EQUAL(ESP_OK,
                          ota_manifest_check_channel(&dev, "production",
                                                     &reason));
    TEST_ASSERT_EQUAL(OTA_REJECT_CHANNEL, reason);

    /* A development unit takes a development image... */
    TEST_ASSERT_EQUAL(ESP_OK, ota_manifest_check_channel(&dev, "dev", &reason));

    /* ...and a more stable one, which is the promotion of the unit. */
    TEST_ASSERT_EQUAL(ESP_OK, ota_manifest_check_channel(&manifest, "dev",
                                                         &reason));

    /* But a beta unit never takes a development image: the image has to be
     * at least as stable as the unit that installs it. */
    TEST_ASSERT_NOT_EQUAL(ESP_OK,
                          ota_manifest_check_channel(&dev, "beta", &reason));
    TEST_ASSERT_EQUAL(OTA_REJECT_CHANNEL, reason);

    /* An unknown channel is refused on both sides: a typo must never widen
     * the acceptance. */
    TEST_ASSERT_EQUAL(OTA_CHANNEL_UNKNOWN, ota_channel_rank("prod"));
    TEST_ASSERT_NOT_EQUAL(ESP_OK,
                          ota_manifest_check_channel(&manifest, "prod", &reason));
    TEST_ASSERT_EQUAL(OTA_REJECT_CHANNEL, reason);

    ota_manifest_t unknown = manifest;
    strcpy(unknown.channel, "nightly");
    TEST_ASSERT_NOT_EQUAL(ESP_OK,
                          ota_manifest_check_channel(&unknown, "dev", &reason));
    TEST_ASSERT_EQUAL(OTA_REJECT_CHANNEL, reason);
}

TEST_CASE("A downgrade is refused", "[ota_manifest]")
{
    ota_reject_reason_t reason = OTA_REJECT_NONE;
    const ota_manifest_t manifest = parse_ok(&reason);

    TEST_ASSERT_EQUAL(ESP_OK,
                      ota_manifest_check_version(&manifest, "1.0.0", &reason));
    TEST_ASSERT_EQUAL(ESP_OK,
                      ota_manifest_check_version(&manifest, "1.1.0", &reason));
    TEST_ASSERT_NOT_EQUAL(ESP_OK,
                          ota_manifest_check_version(&manifest, "1.2.0",
                                                     &reason));
    TEST_ASSERT_EQUAL(OTA_REJECT_DOWNGRADE, reason);

    /* The comparison is numeric, not lexicographic. */
    TEST_ASSERT_TRUE(ota_version_compare("1.10.0", "1.9.0") > 0);
    TEST_ASSERT_TRUE(ota_version_compare("1.0.0", "1.0.0") == 0);
    TEST_ASSERT_TRUE(ota_version_compare("0.9.9", "1.0.0") < 0);
}

TEST_CASE("The digest of the image is compared with the declared one",
          "[ota_manifest]")
{
    ota_reject_reason_t reason = OTA_REJECT_NONE;
    const ota_manifest_t manifest = parse_ok(&reason);

    uint8_t digest[OTA_MANIFEST_HASH_LEN] = {0};
    TEST_ASSERT_EQUAL(ESP_OK, ota_hex_decode(kDigestHex, digest, sizeof(digest)));
    TEST_ASSERT_TRUE(ota_manifest_hash_matches(&manifest, digest));

    digest[0] ^= 0xFFu;
    TEST_ASSERT_FALSE(ota_manifest_hash_matches(&manifest, digest));
}

TEST_CASE("Hexadecimal and base64 decoding", "[ota_manifest]")
{
    uint8_t bytes[4] = {0};

    TEST_ASSERT_EQUAL(ESP_OK, ota_hex_decode("00ff10a0", bytes, sizeof(bytes)));
    TEST_ASSERT_EQUAL_HEX8(0x00u, bytes[0]);
    TEST_ASSERT_EQUAL_HEX8(0xFFu, bytes[1]);
    TEST_ASSERT_EQUAL_HEX8(0x10u, bytes[2]);
    TEST_ASSERT_EQUAL_HEX8(0xA0u, bytes[3]);

    TEST_ASSERT_EQUAL(ESP_ERR_INVALID_SIZE,
                      ota_hex_decode("00ff10", bytes, sizeof(bytes)));
    TEST_ASSERT_EQUAL(ESP_ERR_INVALID_ARG,
                      ota_hex_decode("00ffzza0", bytes, sizeof(bytes)));

    uint8_t decoded[64] = {0};
    size_t written = 0u;
    const char *signature =
        "AAECAwQFBgcICQoLDA0ODxAREhMUFRYXGBkaGxwdHh8gISIjJCUmJygpKissLS4vMDEyMzQ1Njc4OTo7PD0+Pw==";
    TEST_ASSERT_EQUAL(ESP_OK, ota_base64_decode(signature, decoded,
                                                 sizeof(decoded), &written));
    TEST_ASSERT_EQUAL_UINT32(64u, written);
    TEST_ASSERT_EQUAL_HEX8(0x00u, decoded[0]);
    TEST_ASSERT_EQUAL_HEX8(0x3Fu, decoded[63]);

    /* A character outside the alphabet is refused. */
    TEST_ASSERT_EQUAL(ESP_ERR_INVALID_ARG,
                      ota_base64_decode("AA*A", decoded, sizeof(decoded),
                                        &written));
}

TEST_CASE("The signature is verified over the canonical payload", "[ota_manifest]")
{
    ota_reject_reason_t reason = OTA_REJECT_NONE;
    const ota_manifest_t manifest = parse_ok(&reason);

    /* The host build has no cryptography, so the verification is delegated
     * to a double that records what it was asked to check. The point of the
     * test is that the bytes handed to the verifier are the canonical
     * payload and that the signature travels as raw r||s. */
    memset(g_seen_payload, 0, sizeof(g_seen_payload));
    g_seen_signature_len = 0u;
    g_accept_signature = true;

    TEST_ASSERT_EQUAL(ESP_OK,
                      ota_manifest_verify_signature(&manifest, verify_double));
    TEST_ASSERT_EQUAL_UINT32(64u, g_seen_signature_len);
    TEST_ASSERT_EQUAL_STRING(kCanonicalPayload, g_seen_payload);

    /* The signature travels as the raw r||s pair, not as base64 text. */
    TEST_ASSERT_EQUAL_HEX8(0x00u, g_seen_signature[0]);
    TEST_ASSERT_EQUAL_HEX8(0x01u, g_seen_signature[1]);
    TEST_ASSERT_EQUAL_HEX8(0x3Fu, g_seen_signature[63]);

    /* A verifier that refuses must fail the manifest. */
    g_accept_signature = false;
    TEST_ASSERT_NOT_EQUAL(ESP_OK,
                          ota_manifest_verify_signature(&manifest,
                                                       verify_double));
}
