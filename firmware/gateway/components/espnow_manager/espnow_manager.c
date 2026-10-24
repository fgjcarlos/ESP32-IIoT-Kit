#include "espnow_manager.h"

#include <inttypes.h>

#include "esp_log.h"

static const char *TAG = "espnow_manager";

esp_err_t espnow_manager_init(void)
{
    ESP_LOGI(TAG, "ESP-NOW manager contract-only placeholder; runtime not implemented");
    return ESP_OK;
}

esp_err_t espnow_manager_send(const uint8_t *frame, size_t len)
{
    if (frame == NULL || len == 0) {
        return ESP_ERR_INVALID_ARG;
    }
    // The radio path lands with the Fase 1 T1.4.x change. Until then the
    // frame is traced so a command that reached the gateway is still
    // observable in the log.
    ESP_LOGI(TAG, "ESP-NOW send contract: %" PRIu32 " byte frame (radio not implemented)",
             (uint32_t)len);
    return ESP_OK;
}
