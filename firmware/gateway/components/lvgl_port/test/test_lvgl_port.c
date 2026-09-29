#include "unity.h"
#include "lvgl_port.h"

TEST_CASE("LVGL port init returns ESP_OK", "[lvgl_port]")
{
    TEST_ASSERT_EQUAL(ESP_OK, lvgl_port_init());
}

TEST_CASE("LVGL port start returns ESP_OK", "[lvgl_port]")
{
    TEST_ASSERT_EQUAL(ESP_OK, lvgl_port_init());
    TEST_ASSERT_EQUAL(ESP_OK, lvgl_port_start());
}

TEST_CASE("LVGL port stop returns ESP_OK after start", "[lvgl_port]")
{
    TEST_ASSERT_EQUAL(ESP_OK, lvgl_port_init());
    TEST_ASSERT_EQUAL(ESP_OK, lvgl_port_start());
    TEST_ASSERT_EQUAL(ESP_OK, lvgl_port_stop());
}