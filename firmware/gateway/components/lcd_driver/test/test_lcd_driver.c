#include "unity.h"
#include "lcd_driver.h"

TEST_CASE("LCD driver backlight GPIO candidate matches issue-10 design note",
          "[lcd_driver]")
{
    /* GPIO22 is documented as a candidate, not a binding assignment. The
     * value is asserted here to lock the design-time proposal so any future
     * change to a different GPIO is a deliberate header edit, not a silent
     * drift. */
    TEST_ASSERT_EQUAL_INT32(22, LCD_DRIVER_BACKLIGHT_GPIO_CANDIDATE);
}

TEST_CASE("LCD driver contract initializes", "[lcd_driver]")
{
    TEST_ASSERT_EQUAL(ESP_OK, lcd_driver_init());
}