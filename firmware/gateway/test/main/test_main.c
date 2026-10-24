#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "sdkconfig.h"
#include "unity.h"

static const char *TAG = "gateway_test";

void app_main(void)
{
    ESP_LOGI(TAG, "Running gateway board-profile Unity tests");
    UNITY_BEGIN();
    unity_run_all_tests();
    UNITY_END();

    // The measured headroom of the main task: the number of bytes that never
    // got used. It is the evidence behind CONFIG_ESP_MAIN_TASK_STACK_SIZE.
    const UBaseType_t headroom = uxTaskGetStackHighWaterMark(NULL);
    ESP_LOGI(TAG, "Main task stack headroom after the run: %u bytes of %u",
             (unsigned)headroom, (unsigned)CONFIG_ESP_MAIN_TASK_STACK_SIZE);
}
