/**
 * @file ota_manager.c
 * @brief ESP-IDF half of the OTA layer.
 *
 * This file owns everything that needs the device: the cryptography
 * (mbedTLS), the transport (HTTPS with the certificate bundle), the flash
 * (the `esp_ota_ops` API) and the state that has to survive a reboot (the
 * NVS namespace "ota").
 *
 * The decisions — which images may be installed, in which channel, with
 * which digest — live in `ota_manifest`, which is plain C and is exercised
 * on the host. This file executes those decisions.
 *
 * The order of the operations is the security property of the whole
 * feature, so it is worth stating it plainly:
 *
 *   - The manifest is fetched, parsed and **signed-verified** first. A
 *     manifest that does not verify ends the attempt there.
 *   - The compatibility and the channel are checked **before the image is
 *     requested**. A foreign image is refused without a single byte of it
 *     travelling over the network.
 *   - The image is written into the inactive slot, read back, and its
 *     SHA-256 is compared with the digest the signed manifest declares. The
 *     bootloader is only pointed at the slot when the digests agree, so a
 *     corrupted image is never activated.
 */

#include "ota_manager.h"

#include <stdio.h>
#include <string.h>

#include "board_profile.h"
#include "brand_profile.h"
#include "esp_crt_bundle.h"
#include "esp_http_client.h"
#include "esp_log.h"
#include "esp_ota_ops.h"
#include "esp_partition.h"
#include "esp_system.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "mbedtls/bignum.h"
#include "mbedtls/ecdsa.h"
#include "mbedtls/ecp.h"
#include "mbedtls/pk.h"
#include "mbedtls/sha256.h"
#include "nvs.h"
#include "nvs_flash.h"
#include "sdkconfig.h"

/* The public half of the release signing key. A public key is not a secret:
 * it is published with the firmware so the unit can verify the manifests of
 * the releases it is meant to install. The private half never reaches the
 * repository; it lives in the release pipeline. */
extern const char ota_signing_key_pem_start[] asm("_binary_ota_signing_key_pem_start");

/** NVS namespace owned by this module. */
#define OTA_NVS_NAMESPACE "ota"
/** NVS key holding the URL of the release manifest. */
#define OTA_NVS_KEY_MANIFEST_URL "manifest_url"

/** Timeout of a single HTTP request, in milliseconds. */
#define OTA_HTTP_TIMEOUT_MS 15000
/** Stack of the task that performs an update, in bytes.
 *
 * The HTTP client and the TLS session need more than the 4 KiB default of
 * FreeRTOS, so the update never runs on the caller's stack. */
#define OTA_TASK_STACK_SIZE 12288
/** Size of the buffer used to stream the image. */
#define OTA_CHUNK_SIZE 4096
/** Length of the raw ECDSA P-256 signature: r and s, 32 bytes each. */
#define OTA_SIGNATURE_RAW_LEN 64u
/** Length of the embedded public key, including the terminator. */
#define OTA_PUBLIC_KEY_MAX_LEN 512u

static const char *TAG = "ota_manager";

/** State shared between the API and the update task. */
static ota_state_t s_state = OTA_STATE_IDLE;
static bool s_busy;
static bool s_pending_verify;
static bool s_confirmed;
static char s_last_reason[OTA_MANAGER_REASON_LEN] = "none";
static char s_last_error[OTA_MANAGER_ERROR_LEN] = "";
static char s_running_partition[OTA_MANAGER_PARTITION_LABEL_LEN] = "";
static char s_boot_partition[OTA_MANAGER_PARTITION_LABEL_LEN] = "";
static char s_current_version[OTA_MANAGER_VERSION_LEN] = "";
static char s_channel[OTA_MANAGER_CHANNEL_LEN] = "";

/**
 * @brief Arguments of the update task.
 */
typedef struct {
    char url[OTA_MANIFEST_URL_MAX_LEN];
    esp_err_t result;
} ota_task_args_t;

/* ------------------------------------------------------------------ */
/* Cryptography                                                        */
/* ------------------------------------------------------------------ */

/**
 * @brief SHA-256 of a byte range.
 */
static esp_err_t ota_sha256_digest(const void *data, size_t len,
                                   uint8_t out[OTA_MANIFEST_HASH_LEN])
{
    if (data == NULL || out == NULL) {
        return ESP_ERR_INVALID_ARG;
    }

    mbedtls_sha256_context context;
    mbedtls_sha256_init(&context);

    esp_err_t err = ESP_OK;
    if (mbedtls_sha256_starts(&context, 0) != 0) {
        err = ESP_FAIL;
    } else if (mbedtls_sha256_update(&context, (const unsigned char *)data,
                                     len) != 0) {
        err = ESP_FAIL;
    } else if (mbedtls_sha256_finish(&context, out) != 0) {
        err = ESP_FAIL;
    }

    mbedtls_sha256_free(&context);
    return err;
}

/**
 * @brief Verify an ECDSA P-256 signature over a byte range.
 *
 * The signature is the raw `r || s` pair the manifest carries. The public
 * key is the one embedded in the firmware at build time.
 *
 * @param signature     Signature bytes.
 * @param signature_len Length of @p signature; must be 64.
 * @param data          Signed bytes.
 * @param data_len      Length of @p data.
 * @return ESP_OK when the signature is valid.
 */
static esp_err_t ota_ecdsa_verify(const uint8_t *signature, size_t signature_len,
                                  const uint8_t *data, size_t data_len)
{
    if (signature == NULL || data == NULL) {
        return ESP_ERR_INVALID_ARG;
    }
    if (signature_len != OTA_SIGNATURE_RAW_LEN) {
        return ESP_ERR_INVALID_SIZE;
    }

    mbedtls_pk_context key;
    mbedtls_mpi r;
    mbedtls_mpi s;
    mbedtls_sha256_context hash_context;

    mbedtls_pk_init(&key);
    mbedtls_mpi_init(&r);
    mbedtls_mpi_init(&s);
    mbedtls_sha256_init(&hash_context);

    esp_err_t err = ESP_OK;
    uint8_t digest[OTA_MANIFEST_HASH_LEN] = {0};

    const size_t pem_len = strlen(ota_signing_key_pem_start);
    if (mbedtls_pk_parse_public_key(&key,
                                    (const unsigned char *)ota_signing_key_pem_start,
                                    pem_len + 1u) != 0) {
        ESP_LOGE(TAG, "the embedded release key cannot be parsed");
        err = ESP_ERR_INVALID_STATE;
        goto cleanup;
    }

    const mbedtls_ecp_keypair *keypair = mbedtls_pk_ec(key);
    if (keypair == NULL) {
        ESP_LOGE(TAG, "the embedded release key is not an EC key");
        err = ESP_ERR_INVALID_STATE;
        goto cleanup;
    }

    if (mbedtls_mpi_read_binary(&r, signature, 32u) != 0 ||
        mbedtls_mpi_read_binary(&s, signature + 32u, 32u) != 0) {
        err = ESP_ERR_INVALID_ARG;
        goto cleanup;
    }

    if (mbedtls_sha256_starts(&hash_context, 0) != 0 ||
        mbedtls_sha256_update(&hash_context, data, data_len) != 0 ||
        mbedtls_sha256_finish(&hash_context, digest) != 0) {
        err = ESP_FAIL;
        goto cleanup;
    }

    if (mbedtls_ecdsa_verify(&keypair->MBEDTLS_PRIVATE(grp), digest,
                             sizeof(digest), &keypair->MBEDTLS_PRIVATE(Q),
                             &r, &s) != 0) {
        err = ESP_ERR_INVALID_STATE;
    }

cleanup:
    mbedtls_sha256_free(&hash_context);
    mbedtls_mpi_free(&r);
    mbedtls_mpi_free(&s);
    mbedtls_pk_free(&key);
    return err;
}

/* ------------------------------------------------------------------ */
/* Transport                                                           */
/* ------------------------------------------------------------------ */

/**
 * @brief Fill the HTTP configuration with the TLS trust anchor.
 *
 * The certificate bundle of ESP-IDF is the trust anchor: the release
 * server presents a certificate signed by a public authority. A private
 * authority is a deployment decision and is documented in
 * `docs/ota/ota-security.md`.
 */
static void ota_http_config(esp_http_client_config_t *config, const char *url)
{
    memset(config, 0, sizeof(*config));
    config->url = url;
    config->timeout_ms = OTA_HTTP_TIMEOUT_MS;
    config->keep_alive_enable = true;
    config->crt_bundle_attach = esp_crt_bundle_attach;
}

/**
 * @brief Fetch a small document over HTTPS.
 *
 * @param url       URL to fetch. May not be NULL.
 * @param buffer    Destination buffer.
 * @param buffer_len Size of @p buffer.
 * @param out_len   Optional destination for the number of bytes read.
 * @return ESP_OK on success.
 */
static esp_err_t ota_http_fetch(const char *url, char *buffer, size_t buffer_len,
                                size_t *out_len)
{
    esp_http_client_config_t config;
    ota_http_config(&config, url);

    esp_http_client_handle_t client = esp_http_client_init(&config);
    if (client == NULL) {
        return ESP_FAIL;
    }

    esp_err_t err = ESP_OK;
    size_t received = 0u;

    if (esp_http_client_open(client, 0) != ESP_OK) {
        err = ESP_FAIL;
        goto cleanup;
    }

    const int headers = esp_http_client_fetch_headers(client);
    if (headers < 0) {
        ESP_LOGE(TAG, "no response from %s", url);
        err = ESP_FAIL;
        goto cleanup;
    }

    const int status = esp_http_client_get_status_code(client);
    if (status != 200) {
        ESP_LOGE(TAG, "%s answered %d", url, status);
        err = ESP_FAIL;
        goto cleanup;
    }

    while (true) {
        const int read = esp_http_client_read(client, buffer + received,
                                              buffer_len - received - 1u);
        if (read < 0) {
            err = ESP_FAIL;
            goto cleanup;
        }
        if (read == 0) {
            break;
        }
        received += (size_t)read;
        if (received + 1u >= buffer_len) {
            ESP_LOGE(TAG, "the document is larger than the buffer");
            err = ESP_ERR_INVALID_SIZE;
            goto cleanup;
        }
    }

    buffer[received] = '\0';
    if (out_len != NULL) {
        *out_len = received;
    }

cleanup:
    esp_http_client_cleanup(client);
    return err;
}

/**
 * @brief Download an image into an OTA slot.
 *
 * The image is streamed into the slot, never held in RAM.
 *
 * @param url           URL of the image. May not be NULL.
 * @param partition     Slot to write. May not be NULL.
 * @param expected_size Size the manifest declares.
 * @return ESP_OK on success.
 */
static esp_err_t ota_http_download(const char *url,
                                   const esp_partition_t *partition,
                                   uint32_t expected_size)
{
    esp_http_client_config_t config;
    ota_http_config(&config, url);

    esp_http_client_handle_t client = esp_http_client_init(&config);
    if (client == NULL) {
        return ESP_FAIL;
    }

    esp_err_t err = ESP_OK;
    esp_ota_handle_t ota_handle = 0;
    bool ota_open = false;
    uint32_t received = 0u;
    char *buffer = malloc(OTA_CHUNK_SIZE);
    if (buffer == NULL) {
        err = ESP_ERR_NO_MEM;
        goto cleanup;
    }

    if (esp_http_client_open(client, 0) != ESP_OK) {
        err = ESP_FAIL;
        goto cleanup;
    }

    if (esp_http_client_fetch_headers(client) < 0) {
        ESP_LOGE(TAG, "no response from %s", url);
        err = ESP_FAIL;
        goto cleanup;
    }

    const int status = esp_http_client_get_status_code(client);
    if (status != 200) {
        ESP_LOGE(TAG, "%s answered %d", url, status);
        err = ESP_FAIL;
        goto cleanup;
    }

    if (esp_ota_begin(partition, expected_size, &ota_handle) != ESP_OK) {
        ESP_LOGE(TAG, "the slot %s cannot be erased", partition->label);
        err = ESP_FAIL;
        goto cleanup;
    }
    ota_open = true;

    while (true) {
        const int read = esp_http_client_read(client, buffer, OTA_CHUNK_SIZE);
        if (read < 0) {
            ESP_LOGE(TAG, "the download of %s failed", url);
            err = ESP_FAIL;
            goto cleanup;
        }
        if (read == 0) {
            break;
        }

        if (received + (uint32_t)read > expected_size) {
            ESP_LOGE(TAG, "the image is larger than the manifest declares");
            err = ESP_ERR_INVALID_SIZE;
            goto cleanup;
        }

        if (esp_ota_write(ota_handle, buffer, (size_t)read) != ESP_OK) {
            ESP_LOGE(TAG, "the slot %s cannot be written", partition->label);
            err = ESP_FAIL;
            goto cleanup;
        }
        received += (uint32_t)read;
    }

    if (received != expected_size) {
        ESP_LOGE(TAG, "the image is %u bytes, the manifest declares %u",
                 (unsigned int)received, (unsigned int)expected_size);
        err = ESP_ERR_INVALID_SIZE;
        goto cleanup;
    }

    if (esp_ota_end(ota_handle) != ESP_OK) {
        ESP_LOGE(TAG, "the image does not pass the validation of the bootloader");
        err = ESP_ERR_OTA_VALIDATE_FAILED;
        ota_open = false;
        goto cleanup;
    }
    ota_open = false;

    ESP_LOGI(TAG, "the image was written into %s (%u bytes)",
             partition->label, (unsigned int)received);

cleanup:
    if (ota_open) {
        esp_ota_abort(ota_handle);
    }
    free(buffer);
    esp_http_client_cleanup(client);
    return err;
}

/**
 * @brief Digest the content of a partition and compare it with the manifest.
 *
 * The slot is read back from the flash instead of trusting the bytes that
 * were streamed into it: a write error that the flash controller did not
 * report is still caught here.
 *
 * @param partition Slot to read. May not be NULL.
 * @param size      Number of bytes to digest.
 * @param digest    Destination digest. May not be NULL.
 * @return ESP_OK on success.
 */
static esp_err_t ota_partition_digest(const esp_partition_t *partition,
                                       uint32_t size,
                                       uint8_t digest[OTA_MANIFEST_HASH_LEN])
{
    mbedtls_sha256_context context;
    mbedtls_sha256_init(&context);
    if (mbedtls_sha256_starts(&context, 0) != 0) {
        mbedtls_sha256_free(&context);
        return ESP_FAIL;
    }

    char *buffer = malloc(OTA_CHUNK_SIZE);
    if (buffer == NULL) {
        mbedtls_sha256_free(&context);
        return ESP_ERR_NO_MEM;
    }

    esp_err_t err = ESP_OK;
    uint32_t offset = 0u;
    while (offset < size) {
        const uint32_t chunk =
            (size - offset < OTA_CHUNK_SIZE) ? (size - offset) : OTA_CHUNK_SIZE;
        if (esp_partition_read(partition, offset, buffer, chunk) != ESP_OK) {
            err = ESP_FAIL;
            break;
        }
        if (mbedtls_sha256_update(&context, (const unsigned char *)buffer,
                                  chunk) != 0) {
            err = ESP_FAIL;
            break;
        }
        offset += chunk;
    }

    if (err == ESP_OK && mbedtls_sha256_finish(&context, digest) != 0) {
        err = ESP_FAIL;
    }

    free(buffer);
    mbedtls_sha256_free(&context);
    return err;
}

/* ------------------------------------------------------------------ */
/* State                                                               */
/* ------------------------------------------------------------------ */

/**
 * @brief Record the outcome of the last attempt.
 */
static void ota_record_failure(ota_reject_reason_t reason, esp_err_t err)
{
    s_state = OTA_STATE_FAILED;
    snprintf(s_last_reason, sizeof(s_last_reason), "%s",
             ota_reject_reason_name(reason));
    snprintf(s_last_error, sizeof(s_last_error), "%s", esp_err_to_name(err));
    ESP_LOGE(TAG, "the update was refused (%s): %s",
             ota_reject_reason_name(reason), esp_err_to_name(err));
}

/**
 * @brief Build the identity of this unit from the sources that own it.
 */
static void ota_fill_identity(ota_device_identity_t *identity)
{
    const brand_profile_t *profile = brand_profile_get();

    identity->product = profile->product_id;
    identity->brand = profile->brand_name;
    identity->model = profile->model;
    identity->revision = profile->hw_revision;
    identity->target = CONFIG_IDF_TARGET;
    identity->version = profile->firmware_version;
    identity->channel = profile->ota_channel;
}

/* ------------------------------------------------------------------ */
/* The update                                                          */
/* ------------------------------------------------------------------ */

/**
 * @brief Run the whole update.
 *
 * @param url URL of the release manifest.
 * @return ESP_OK when the new image was installed and armed.
 */
static esp_err_t ota_run_update(const char *url)
{
    char document[OTA_MANIFEST_JSON_MAX_LEN] = {0};
    size_t document_len = 0u;

    s_state = OTA_STATE_CHECKING;

    /* 1. The manifest. */
    esp_err_t err = ota_http_fetch(url, document, sizeof(document),
                                    &document_len);
    if (err != ESP_OK) {
        ota_record_failure(OTA_REJECT_MALFORMED, err);
        return err;
    }

    /* 2. Its shape. */
    ota_manifest_t manifest;
    ota_reject_reason_t reason = OTA_REJECT_MALFORMED;
    err = ota_manifest_parse(document, document_len, &manifest, &reason);
    if (err != ESP_OK) {
        ota_record_failure(reason, err);
        return err;
    }

    /* 3. Its signature. An unsigned manifest is never acted upon. */
    err = ota_manifest_verify_signature(&manifest, ota_ecdsa_verify);
    if (err != ESP_OK) {
        ota_record_failure(OTA_REJECT_SIGNATURE, err);
        return err;
    }

    /* 4. The optional digest of the canonical form, when the release
     *    publishes one. */
    if (manifest.manifest_sha256[0] != '\0') {
        uint8_t digest[OTA_MANIFEST_HASH_LEN] = {0};
        err = ota_manifest_digest(&manifest, ota_sha256_digest, digest);
        if (err != ESP_OK) {
            ota_record_failure(OTA_REJECT_MANIFEST_HASH, err);
            return err;
        }
        uint8_t declared[OTA_MANIFEST_HASH_LEN] = {0};
        if (ota_hex_decode(manifest.manifest_sha256, declared,
                           sizeof(declared)) != ESP_OK ||
            memcmp(declared, digest, sizeof(digest)) != 0) {
            ota_record_failure(OTA_REJECT_MANIFEST_HASH,
                               ESP_ERR_INVALID_STATE);
            return ESP_ERR_INVALID_STATE;
        }
    }

    ESP_LOGI(TAG, "manifest accepted: product=%s brand=%s model=%s rev=%s "
                  "version=%s channel=%s key=%s",
             manifest.product, manifest.brand, manifest.model, manifest.revision,
             manifest.version, manifest.channel, manifest.key_id);

    /* 5. The identity of the unit. Nothing of the image is fetched before
     *    this point. */
    ota_device_identity_t identity;
    ota_fill_identity(&identity);

    err = ota_manifest_check_compatible(&manifest, &identity, &reason);
    if (err != ESP_OK) {
        ota_record_failure(reason, err);
        return err;
    }

    err = ota_manifest_check_channel(&manifest, identity.channel, &reason);
    if (err != ESP_OK) {
        ota_record_failure(reason, err);
        return err;
    }

    err = ota_manifest_check_version(&manifest, identity.version, &reason);
    if (err != ESP_OK) {
        ota_record_failure(reason, err);
        return err;
    }

    /* 6. The slot that will receive the image. */
    const esp_partition_t *slot = esp_ota_get_next_update_partition(NULL);
    if (slot == NULL) {
        ota_record_failure(OTA_REJECT_IMAGE_SIZE, ESP_ERR_NOT_FOUND);
        return ESP_ERR_NOT_FOUND;
    }
    if (manifest.size > slot->size) {
        ESP_LOGE(TAG, "the image is %u bytes, the slot %s holds %u",
                 (unsigned int)manifest.size, slot->label,
                 (unsigned int)slot->size);
        ota_record_failure(OTA_REJECT_IMAGE_SIZE, ESP_ERR_INVALID_SIZE);
        return ESP_ERR_INVALID_SIZE;
    }

    /* 7. The image itself. */
    s_state = OTA_STATE_DOWNLOADING;
    err = ota_http_download(manifest.url, slot, manifest.size);
    if (err != ESP_OK) {
        ota_record_failure(OTA_REJECT_IMAGE_HASH, err);
        return err;
    }

    /* 8. The digest of what really landed in the flash. */
    s_state = OTA_STATE_VERIFYING;
    uint8_t digest[OTA_MANIFEST_HASH_LEN] = {0};
    err = ota_partition_digest(slot, manifest.size, digest);
    if (err != ESP_OK) {
        ota_record_failure(OTA_REJECT_IMAGE_HASH, err);
        return err;
    }
    if (!ota_manifest_hash_matches(&manifest, digest)) {
        ESP_LOGE(TAG, "the image does not match the digest of the manifest");
        ota_record_failure(OTA_REJECT_IMAGE_HASH, ESP_ERR_INVALID_STATE);
        return ESP_ERR_INVALID_STATE;
    }
    ESP_LOGI(TAG, "the image matches the digest of the manifest");

    /* 9. The bootloader is pointed at the new slot. The image boots in the
     *    PENDING_VERIFY state: it only becomes permanent when the
     *    application confirms it. */
    err = esp_ota_set_boot_partition(slot);
    if (err != ESP_OK) {
        ota_record_failure(OTA_REJECT_IMAGE_HASH, err);
        return err;
    }

    s_state = OTA_STATE_READY_TO_REBOOT;
    snprintf(s_last_reason, sizeof(s_last_reason), "none");
    snprintf(s_last_error, sizeof(s_last_error), "none");
    ESP_LOGI(TAG, "the image %s is installed in %s: rebooting into it",
             manifest.version, slot->label);

    vTaskDelay(pdMS_TO_TICKS(200));
    esp_restart();

    return ESP_OK;
}

/**
 * @brief Body of the update task.
 */
static void ota_update_task(void *arg)
{
    ota_task_args_t *args = arg;

    args->result = ota_run_update(args->url);

    s_busy = false;
    free(args);
    vTaskDelete(NULL);
}

/**
 * @brief Body of the confirmation watchdog.
 *
 * A new image that hangs never reaches the point where it confirms itself.
 * The watchdog reboots the unit, and the bootloader rolls the image back
 * because it is still in the PENDING_VERIFY state.
 */
static void ota_watchdog_task(void *arg)
{
    (void)arg;

    for (int elapsed = 0; elapsed < OTA_MANAGER_CONFIRM_WINDOW_SEC; elapsed++) {
        if (s_confirmed) {
            vTaskDelete(NULL);
            return;
        }
        vTaskDelay(pdMS_TO_TICKS(1000));
    }

    if (!s_confirmed) {
        ESP_LOGE(TAG, "the image did not confirm itself within %d s: "
                      "rebooting so the bootloader rolls it back",
                 OTA_MANAGER_CONFIRM_WINDOW_SEC);
        fflush(stdout);
        esp_restart();
    }

    vTaskDelete(NULL);
}

/* ------------------------------------------------------------------ */
/* Public API                                                          */
/* ------------------------------------------------------------------ */

esp_err_t ota_manager_init(void)
{
    const brand_profile_t *profile = brand_profile_get();
    snprintf(s_current_version, sizeof(s_current_version), "%s",
             profile->firmware_version);
    snprintf(s_channel, sizeof(s_channel), "%s", profile->ota_channel);

    const esp_partition_t *running = esp_ota_get_running_partition();
    if (running != NULL) {
        snprintf(s_running_partition, sizeof(s_running_partition), "%s",
                 running->label);
    }
    const esp_partition_t *boot = esp_ota_get_boot_partition();
    if (boot != NULL) {
        snprintf(s_boot_partition, sizeof(s_boot_partition), "%s", boot->label);
    }

    esp_ota_img_states_t state = ESP_OTA_IMG_UNDEFINED;
    if (running != NULL &&
        esp_ota_get_state_partition(running, &state) == ESP_OK) {
        s_pending_verify = (state == ESP_OTA_IMG_PENDING_VERIFY);
    }

    if (s_pending_verify) {
        ESP_LOGW(TAG, "the running image %s is on trial: it is rolled back if "
                      "it does not confirm itself within %d s",
                 s_running_partition, OTA_MANAGER_CONFIRM_WINDOW_SEC);

        if (xTaskCreate(ota_watchdog_task, "ota_watchdog",
                        OTA_TASK_STACK_SIZE, NULL, 5, NULL) != pdPASS) {
            ESP_LOGE(TAG, "the confirmation watchdog could not be started");
            return ESP_ERR_NO_MEM;
        }
    } else {
        ESP_LOGI(TAG, "ota: running %s, boot %s, version %s, channel %s",
                 s_running_partition, s_boot_partition, s_current_version,
                 s_channel);
    }

    return ESP_OK;
}

esp_err_t ota_manager_start_update(const char *manifest_url)
{
    if (manifest_url == NULL || manifest_url[0] == '\0') {
        return ESP_ERR_INVALID_ARG;
    }
    if (s_busy) {
        ESP_LOGW(TAG, "an update is already running");
        return ESP_ERR_INVALID_STATE;
    }

    ota_task_args_t *args = calloc(1, sizeof(*args));
    if (args == NULL) {
        return ESP_ERR_NO_MEM;
    }
    snprintf(args->url, sizeof(args->url), "%s", manifest_url);

    s_busy = true;
    s_state = OTA_STATE_CHECKING;
    if (xTaskCreate(ota_update_task, "ota_update", OTA_TASK_STACK_SIZE, args,
                    5, NULL) != pdPASS) {
        s_busy = false;
        free(args);
        return ESP_ERR_NO_MEM;
    }

    ESP_LOGI(TAG, "the update of %s has started", manifest_url);
    return ESP_OK;
}

esp_err_t ota_manager_start_configured_update(void)
{
    char url[OTA_MANIFEST_URL_MAX_LEN] = {0};
    const esp_err_t err = ota_manager_get_manifest_url(url, sizeof(url));
    if (err != ESP_OK) {
        ESP_LOGW(TAG, "no release server has been provisioned");
        return err;
    }
    return ota_manager_start_update(url);
}

bool ota_manager_is_busy(void)
{
    return s_busy;
}

esp_err_t ota_manager_verify_manifest(const ota_manifest_t *manifest)
{
    if (manifest == NULL) {
        return ESP_ERR_INVALID_ARG;
    }
    return ota_manifest_verify_signature(manifest, ota_ecdsa_verify);
}

esp_err_t ota_manager_get_status(ota_status_t *out)
{
    if (out == NULL) {
        return ESP_ERR_INVALID_ARG;
    }

    memset(out, 0, sizeof(*out));
    out->state = s_state;
    out->busy = s_busy;
    out->pending_verify = s_pending_verify;
    snprintf(out->running_partition, sizeof(out->running_partition), "%s",
             s_running_partition);
    snprintf(out->boot_partition, sizeof(out->boot_partition), "%s",
             s_boot_partition);
    snprintf(out->current_version, sizeof(out->current_version), "%s",
             s_current_version);
    snprintf(out->channel, sizeof(out->channel), "%s", s_channel);
    snprintf(out->last_reason, sizeof(out->last_reason), "%s", s_last_reason);
    snprintf(out->last_error, sizeof(out->last_error), "%s", s_last_error);
    return ESP_OK;
}

esp_err_t ota_manager_confirm_pending_image(void)
{
    if (!s_pending_verify) {
        return ESP_ERR_INVALID_STATE;
    }

    const esp_err_t err = esp_ota_mark_app_valid_cancel_rollback();
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "the image could not be confirmed: %s",
                 esp_err_to_name(err));
        return err;
    }

    s_confirmed = true;
    s_pending_verify = false;
    ESP_LOGI(TAG, "the image %s is confirmed: the rollback is cancelled",
             s_running_partition);
    return ESP_OK;
}

bool ota_manager_image_pending(void)
{
    return s_pending_verify;
}

esp_err_t ota_manager_get_manifest_url(char *out, size_t out_len)
{
    if (out == NULL) {
        return ESP_ERR_INVALID_ARG;
    }

    nvs_handle_t handle = 0;
    esp_err_t err = nvs_open(OTA_NVS_NAMESPACE, NVS_READONLY, &handle);
    if (err != ESP_OK) {
        return err;
    }

    size_t length = out_len;
    err = nvs_get_str(handle, OTA_NVS_KEY_MANIFEST_URL, out, &length);
    nvs_close(handle);

    if (err == ESP_ERR_NVS_NOT_FOUND) {
        return ESP_ERR_NOT_FOUND;
    }
    return err;
}

esp_err_t ota_manager_set_manifest_url(const char *url)
{
    if (url == NULL || url[0] == '\0') {
        return ESP_ERR_INVALID_ARG;
    }

    nvs_handle_t handle = 0;
    esp_err_t err = nvs_open(OTA_NVS_NAMESPACE, NVS_READWRITE, &handle);
    if (err != ESP_OK) {
        return err;
    }

    err = nvs_set_str(handle, OTA_NVS_KEY_MANIFEST_URL, url);
    if (err == ESP_OK) {
        err = nvs_commit(handle);
    }
    nvs_close(handle);
    return err;
}
