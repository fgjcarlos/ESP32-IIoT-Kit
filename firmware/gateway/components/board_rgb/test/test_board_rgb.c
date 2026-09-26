#include "unity.h"
#include "board_rgb.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

TEST_CASE("Onboard WS2812 encoder uses GRB byte order before the GPIO48 smoke test", "[board_rgb]")
{
    uint8_t grb[3];

    TEST_ASSERT_EQUAL(ESP_OK, board_rgb_encode_grb(0xFF, 0x00, 0x00, grb));
    TEST_ASSERT_EQUAL_UINT8(0x00, grb[0]);
    TEST_ASSERT_EQUAL_UINT8(0xFF, grb[1]);
    TEST_ASSERT_EQUAL_UINT8(0x00, grb[2]);

    TEST_ASSERT_EQUAL(ESP_OK, board_rgb_encode_grb(0x00, 0xFF, 0x00, grb));
    TEST_ASSERT_EQUAL_UINT8(0xFF, grb[0]);
    TEST_ASSERT_EQUAL_UINT8(0x00, grb[1]);
    TEST_ASSERT_EQUAL_UINT8(0x00, grb[2]);

    TEST_ASSERT_EQUAL(ESP_OK, board_rgb_encode_grb(0x00, 0x00, 0xFF, grb));
    TEST_ASSERT_EQUAL_UINT8(0x00, grb[0]);
    TEST_ASSERT_EQUAL_UINT8(0x00, grb[1]);
    TEST_ASSERT_EQUAL_UINT8(0xFF, grb[2]);

    const TickType_t smoke_start_ticks = xTaskGetTickCount();

    TEST_ASSERT_EQUAL(ESP_OK, board_rgb_run_smoke_test());

    const TickType_t smoke_elapsed_ticks = xTaskGetTickCount() - smoke_start_ticks;
    TEST_ASSERT_GREATER_OR_EQUAL_UINT32(pdMS_TO_TICKS(4U * 1000U), smoke_elapsed_ticks);
}
