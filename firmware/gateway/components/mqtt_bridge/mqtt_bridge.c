#include "mqtt_bridge.h"

#include "esp_log.h"

static const char *TAG = "mqtt_bridge";

esp_err_t mqtt_bridge_start(void)
{
    ESP_LOGI(TAG, "MQTT bridge start is an interface-only placeholder");
    return ESP_OK;
}

esp_err_t mqtt_bridge_stop(void)
{
    ESP_LOGI(TAG, "MQTT bridge stop is an interface-only placeholder");
    return ESP_OK;
}
