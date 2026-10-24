#include "mqtt_bridge.h"

#include "esp_log.h"

#if CONFIG_SPARKPLUG_B_ENABLED
#include "sparkplug_b.h"
#endif

static const char *TAG = "mqtt_bridge";

esp_err_t mqtt_bridge_start(void)
{
#if CONFIG_SPARKPLUG_B_ENABLED
    return sparkplug_b_start();
#else
    ESP_LOGI(TAG, "MQTT bridge start is an interface-only placeholder");
    return ESP_OK;
#endif
}

esp_err_t mqtt_bridge_stop(void)
{
#if CONFIG_SPARKPLUG_B_ENABLED
    return sparkplug_b_stop();
#else
    ESP_LOGI(TAG, "MQTT bridge stop is an interface-only placeholder");
    return ESP_OK;
#endif
}
