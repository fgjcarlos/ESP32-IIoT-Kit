#include "http_server.h"

#include "brand_profile.h"
#include "esp_log.h"

static const char *TAG = "http_server";

esp_err_t http_server_start(void)
{
    ESP_LOGI(TAG, "HTTP server start is a contract-only placeholder");
    return ESP_OK;
}

esp_err_t http_server_stop(void)
{
    ESP_LOGI(TAG, "HTTP server stop is a contract-only placeholder");
    return ESP_OK;
}

esp_err_t http_server_brand_payload(char *out, size_t out_len)
{
    if (out == NULL) {
        return ESP_ERR_INVALID_ARG;
    }

    /* Issue #12: the brand endpoint serves the identity compiled into the
     * firmware. The Fase 4 runtime registers the `/api/brand` URI handler
     * and hands it the bytes produced here; building the payload here keeps
     * the API, the LCD and the logs showing the same identity. */
    return brand_profile_format_json(brand_profile_get(), out, out_len);
}
