#include "assets_brand.h"

#include "esp_log.h"

static const char *TAG = "assets_brand";

esp_err_t assets_brand_init(void)
{
    ESP_LOGI(TAG,
             "assets_brand contract placeholder (label=\"%s\"; no image "
             "decode, no SPIFFS/FATFS read, no draw buffer in this "
             "contract)",
             ASSETS_BRAND_PLACEHOLDER_LABEL);
    return ESP_OK;
}