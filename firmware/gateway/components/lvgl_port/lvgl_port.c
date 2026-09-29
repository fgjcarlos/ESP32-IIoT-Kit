#include "lvgl_port.h"

#include "esp_log.h"

static const char *TAG = "lvgl_port";

esp_err_t lvgl_port_init(void)
{
    ESP_LOGI(TAG,
             "lvgl_port contract placeholder (no lv_init, no display driver, "
             "no draw buffers, no tick timer in this contract)");
    return ESP_OK;
}

esp_err_t lvgl_port_start(void)
{
    ESP_LOGI(TAG,
             "lvgl_port contract placeholder (start: no lv_task_handler, "
             "no FreeRTOS task in this contract)");
    return ESP_OK;
}

esp_err_t lvgl_port_stop(void)
{
    ESP_LOGI(TAG,
             "lvgl_port contract placeholder (stop: no task delete, no LVGL "
             "teardown in this contract)");
    return ESP_OK;
}