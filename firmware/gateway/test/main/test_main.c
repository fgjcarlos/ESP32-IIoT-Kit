#include "esp_log.h"
#include "esp_system.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "sdkconfig.h"
#include "unity.h"

static const char *TAG = "gateway_test";

void app_main(void)
{
    ESP_LOGI(TAG, "Running gateway board-profile Unity tests");

    UNITY_BEGIN();
#if CONFIG_GATEWAY_TEST_PROFILE_LOGIC
    // The CI profile: every test except the ones tagged [hw], which need the
    // physical gateway (the Octal PSRAM, the RMT of the RGB LED, the radio).
    // The separation is mechanical, not a convention: the same test files run
    // in both profiles and only the hardware ones are left out.
    unity_run_tests_by_tag("[hw]", true);
#else
    unity_run_all_tests();
#endif
    UNITY_END();

    // The measured headroom of the main task: the number of bytes that never
    // got used. It is the evidence behind CONFIG_ESP_MAIN_TASK_STACK_SIZE.
    const UBaseType_t headroom = uxTaskGetStackHighWaterMark(NULL);
    ESP_LOGI(TAG, "Main task stack headroom after the run: %u bytes of %u",
             (unsigned)headroom, (unsigned)CONFIG_ESP_MAIN_TASK_STACK_SIZE);

    // The run ends with a restart, which is how ESP-IDF ends a Unity run.
    //
    // The reason is that "the suite finished" has to be an event the
    // environment can observe. If the application simply returned, the chip
    // would sit in the idle task and the emulation would keep running until
    // the CI timeout killed it: a passing run would be reported as a
    // timeout. Before the stack of the main task was fixed, the run ended
    // with a stack overflow, and the panic did the restart that nobody had
    // asked for. The dependency was invisible and it was wrong.
    //
    // A restart is not a panic: the ESP-IDF panic handler prints "***ERROR***"
    // and "Rebooting...", and esp_restart() prints neither. The CI gate
    // reads the difference, so the end of a healthy run is not mistaken for
    // a crash.
    ESP_LOGI(TAG, "The suite finished; restarting the unit");
    esp_restart();
}
