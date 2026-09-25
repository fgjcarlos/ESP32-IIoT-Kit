#include "esp_log.h"
#include "unity.h"

static const char *TAG = "gateway_test";

void app_main(void)
{
    ESP_LOGI(TAG, "Running gateway board-profile Unity tests");
    UNITY_BEGIN();
    unity_run_all_tests();
    UNITY_END();
}
