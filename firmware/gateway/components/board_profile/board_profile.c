#include "board_profile.h"

#include "esp_flash.h"
#include "esp_psram.h"

esp_err_t board_profile_read(board_profile_t *profile)
{
    if (profile == NULL) {
        return ESP_ERR_INVALID_ARG;
    }

    esp_chip_info_t chip_info;
    esp_chip_info(&chip_info);
    profile->chip_model = chip_info.model;

    esp_err_t err = esp_flash_get_physical_size(NULL, &profile->flash_size_bytes);
    if (err != ESP_OK) {
        return err;
    }

    if (!esp_psram_is_initialized()) {
        return ESP_ERR_INVALID_STATE;
    }

    profile->psram_size_bytes = esp_psram_get_size();
    return profile->psram_size_bytes == 0 ? ESP_ERR_INVALID_SIZE : ESP_OK;
}
