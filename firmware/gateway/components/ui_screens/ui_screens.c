#include "ui_screens.h"

#include "esp_log.h"

static const char *TAG = "ui_screens";

esp_err_t ui_screens_init(void)
{
    ESP_LOGI(TAG,
             "ui_screens contract placeholder (no lv_obj_create, no screen "
             "registration, no event binding in this contract; six screen "
             "stubs declared in header)");
    return ESP_OK;
}

void ui_boot_screen(void)
{
    ESP_LOGI(TAG, "show: boot");
}

void ui_status_screen(void)
{
    ESP_LOGI(TAG, "show: status");
}

void ui_connectivity_screen(void)
{
    ESP_LOGI(TAG, "show: connectivity");
}

void ui_nodes_screen(void)
{
    ESP_LOGI(TAG, "show: nodes");
}

void ui_alerts_screen(void)
{
    ESP_LOGI(TAG, "show: alerts");
}

void ui_ota_screen(void)
{
    ESP_LOGI(TAG, "show: ota");
}