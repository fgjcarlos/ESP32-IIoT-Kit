#include "sparkplug_b.h"

#include "esp_log.h"

#define TAG "sparkplug_b"

esp_err_t sparkplug_b_mqtt_start(void);
esp_err_t sparkplug_b_mqtt_stop(void);

static bool s_running;

esp_err_t sparkplug_b_start(void)
{
    if (s_running) {
        ESP_LOGW(TAG, "start() called while already running");
        return ESP_OK;
    }
    esp_err_t err = sparkplug_b_mqtt_start();
    if (err == ESP_OK) s_running = true;
    return err;
}

esp_err_t sparkplug_b_stop(void)
{
    if (!s_running) return ESP_OK;
    esp_err_t err = sparkplug_b_mqtt_stop();
    if (err == ESP_OK) s_running = false;
    return err;
}
