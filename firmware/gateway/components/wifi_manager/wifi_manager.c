/**
 * @file wifi_manager.c
 * @brief AP+STA lifecycle for the IIoT gateway (Phase 0).
 *
 * This file is intentionally a TDD RED scaffold. Every public helper
 * returns ESP_ERR_NOT_FINISHED to keep the Unity suite red until WUC2
 * implements the behaviour. `wifi_manager_init` also returns the same
 * code so the on-target smoke cannot accidentally pass.
 */

#include "wifi_manager.h"

#include <string.h>

#include "esp_log.h"

static const char *TAG = "wifi_manager";

esp_err_t wifi_manager_validate_ap_channel(uint8_t channel, bool *is_valid)
{
    if (is_valid == NULL) {
        return ESP_ERR_INVALID_ARG;
    }
    *is_valid = false;
    return ESP_ERR_NOT_FINISHED;
}

esp_err_t wifi_manager_ssid_in_range(const char *ssid)
{
    return ESP_ERR_NOT_FINISHED;
}

esp_err_t wifi_manager_password_in_range(const char *password)
{
    return ESP_ERR_NOT_FINISHED;
}

esp_err_t wifi_manager_format_mac(const uint8_t mac[6], char *out, size_t out_len)
{
    if (mac == NULL || out == NULL || out_len < WIFI_MANAGER_MAC_STR_LEN) {
        return ESP_ERR_INVALID_ARG;
    }
    memset(out, 0, out_len);
    return ESP_ERR_NOT_FINISHED;
}

esp_err_t wifi_manager_apply_credentials(const wifi_manager_credentials_t *in,
                                         wifi_manager_credentials_t *out)
{
    if (in == NULL || out == NULL) {
        return ESP_ERR_INVALID_ARG;
    }
    memset(out, 0, sizeof(*out));
    return ESP_ERR_NOT_FINISHED;
}

esp_err_t wifi_manager_init(const wifi_manager_credentials_t *credentials)
{
    (void)credentials;
    ESP_LOGE(TAG, "wifi_manager_init: WUC1 RED scaffold, not yet implemented");
    return ESP_ERR_NOT_FINISHED;
}

esp_err_t wifi_manager_deinit(void)
{
    return ESP_ERR_NOT_FINISHED;
}
