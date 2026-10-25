#include "unity.h"

#include "ota_manager.h"

#include <string.h>

#include "nvs_flash.h"

/* The manifest of the golden vector of
 * `firmware/gateway/components/ota_manifest/test/test_ota_manifest.c`, with
 * the signature that the release key of this repository produced over its
 * canonical form.
 *
 * The signature is the one the *embedded* public key has to accept, so this
 * test exercises the real cryptography of the device: mbedTLS parses the
 * key, hashes the canonical payload and verifies the signature. */
static const char *kSignedManifestJson =
    "{\"schema\":1,"
    "\"product\":\"iiot-kit-gateway\","
    "\"brand\":\"IIoT-Kit\","
    "\"model\":\"GW-S3-1\","
    "\"revision\":\"rev-a\","
    "\"target\":\"esp32s3\","
    "\"version\":\"1.1.0\","
    "\"channel\":\"production\","
    "\"size\":1011344,"
    "\"sha256\":\"2f2d5b61c3a9e4f0d8b7a6c5e4d3c2b1a09f8e7d6c5b4a39281706f5e4d3c2b1\","
    "\"url\":\"https://example.org/firmware/gateway.bin\","
    "\"signature\":\"cVTDmC1E7Su2tPhjyKAD5U8/5iymIi3QTTZpFdj4pGEn1lrzxfdE/jxSehdCIa8GG75ThlVh79iqt/5PFs3Bsw==\","
    "\"key_id\":\"ota-2026\","
    "\"manifest_sha256\":\"17c0a482a85cbaf5b6dccccedfcb6cba562df78e5af3215497f0f306bb4d5888\"}";

TEST_CASE("OTA manager initializes and reports the state of the unit",
          "[ota_manager]")
{
    TEST_ASSERT_EQUAL(ESP_OK, ota_manager_init());

    ota_status_t status;
    TEST_ASSERT_EQUAL(ESP_OK, ota_manager_get_status(&status));
    TEST_ASSERT_TRUE(strlen(status.running_partition) > 0u);
    TEST_ASSERT_TRUE(strlen(status.current_version) > 0u);
    TEST_ASSERT_TRUE(strlen(status.channel) > 0u);

    /* An update is not running just because the manager started. */
    TEST_ASSERT_FALSE(ota_manager_is_busy());
}

TEST_CASE("A manifest signed by the release key is accepted", "[ota_manager]")
{
    TEST_ASSERT_EQUAL(ESP_OK, ota_manager_init());

    ota_manifest_t manifest;
    ota_reject_reason_t reason = OTA_REJECT_MALFORMED;
    TEST_ASSERT_EQUAL(ESP_OK,
                      ota_manifest_parse(kSignedManifestJson,
                                         strlen(kSignedManifestJson),
                                         &manifest, &reason));

    /* The signature is verified with the public key embedded in the
     * firmware, over the canonical form of the manifest. */
    TEST_ASSERT_EQUAL(ESP_OK, ota_manager_verify_manifest(&manifest));
}

TEST_CASE("A manifest that was altered after the signature is refused",
          "[ota_manager]")
{
    TEST_ASSERT_EQUAL(ESP_OK, ota_manager_init());

    ota_manifest_t manifest;
    ota_reject_reason_t reason = OTA_REJECT_MALFORMED;
    TEST_ASSERT_EQUAL(ESP_OK,
                      ota_manifest_parse(kSignedManifestJson,
                                         strlen(kSignedManifestJson),
                                         &manifest, &reason));
    TEST_ASSERT_EQUAL(ESP_OK, ota_manager_verify_manifest(&manifest));

    /* Changing one byte of the payload invalidates the signature: the
     * version is the easiest field to tamper with. */
    manifest.version[0] = '9';
    TEST_ASSERT_NOT_EQUAL(ESP_OK, ota_manager_verify_manifest(&manifest));

    /* And so does a different signature. */
    manifest.version[0] = '1';
    manifest.signature[0] = 'A';
    TEST_ASSERT_NOT_EQUAL(ESP_OK, ota_manager_verify_manifest(&manifest));
}

TEST_CASE("The release server URL is provisioned in NVS", "[ota_manager]")
{
    TEST_ASSERT_EQUAL(ESP_OK, ota_manager_init());

    /* The test application does not boot the gateway, so the NVS partition
     * has to be opened before the manager can store anything in it. */
    esp_err_t nvs_err = nvs_flash_init();
    TEST_ASSERT_TRUE(nvs_err == ESP_OK || nvs_err == ESP_ERR_NVS_NO_FREE_PAGES);
    if (nvs_err == ESP_ERR_NVS_NO_FREE_PAGES) {
        TEST_ASSERT_EQUAL(ESP_OK, nvs_flash_erase());
        TEST_ASSERT_EQUAL(ESP_OK, nvs_flash_init());
    }

    const char *url = "https://releases.example.org/iiot-kit/manifest.json";
    TEST_ASSERT_EQUAL(ESP_OK, ota_manager_set_manifest_url(url));

    char readback[OTA_MANIFEST_URL_MAX_LEN] = {0};
    TEST_ASSERT_EQUAL(ESP_OK, ota_manager_get_manifest_url(readback,
                                                           sizeof(readback)));
    TEST_ASSERT_EQUAL_STRING(url, readback);

    /* A URL that is not a URL is refused before it reaches the flash. */
    TEST_ASSERT_EQUAL(ESP_ERR_INVALID_ARG,
                      ota_manager_set_manifest_url(""));
    TEST_ASSERT_EQUAL(ESP_ERR_INVALID_ARG,
                      ota_manager_get_manifest_url(NULL, 0u));
}

TEST_CASE("An image that is not on trial cannot be confirmed", "[ota_manager]")
{
    TEST_ASSERT_EQUAL(ESP_OK, ota_manager_init());

    /* The image running in the test build is not on trial, so there is
     * nothing to confirm. */
    if (!ota_manager_image_pending()) {
        TEST_ASSERT_EQUAL(ESP_ERR_INVALID_STATE,
                          ota_manager_confirm_pending_image());
    }
}
