#include "esp_log.h"
#include "board_profile.h"

static const char *TAG = "gateway";

void app_main(void)
{
    board_profile_t profile;
    const esp_err_t err = board_profile_read(&profile);

    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Board profile read failed: %s", esp_err_to_name(err));
        return;
    }

    ESP_LOGI(TAG, "ESP32-S3 profile: flash=%lu bytes, PSRAM=%u bytes",
             (unsigned long)profile.flash_size_bytes,
             (unsigned int)profile.psram_size_bytes);
}
