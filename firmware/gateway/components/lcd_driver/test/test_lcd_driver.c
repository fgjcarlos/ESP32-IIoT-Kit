#include <stdbool.h>
#include <stddef.h>

#include "unity.h"
#include "lcd_driver.h"

TEST_CASE("LCD driver backlight GPIO candidate is the provisional GPIO7",
          "[lcd_driver]")
{
    /* GPIO7 is a provisional candidate, not a binding assignment. The
     * ESP32-S3-WROOM-1 module does not expose GPIO22, so the withdrawn
     * issue-10 candidate could never be wired (issue #67). The replacement
     * stays unapproved until the carrier and the panel's BLK interface are
     * verified. The value is asserted here to lock the design-time proposal
     * so any future change to a different GPIO is a deliberate header edit,
     * not a silent drift. */
    TEST_ASSERT_EQUAL_INT32(7, LCD_DRIVER_BACKLIGHT_GPIO_CANDIDATE);
}

TEST_CASE("LCD driver contract initializes", "[lcd_driver]")
{
    TEST_ASSERT_EQUAL(ESP_OK, lcd_driver_init());
}

TEST_CASE("LCD candidate is exposed and avoids reserved S3 pins", "[lcd_driver]")
{
    /* Independent-verifier regression (issue #67). The candidate must be a pin
     * the ESP32-S3-WROOM-1 module actually exposes, and it must not be one of
     * the reserved pins. The exposure rule follows the module's pin list
     * (IO0-IO21, IO35-IO48); the reserved set removes the pins inside those
     * ranges that are spoken for: Octal PSRAM, native USB, UART0, strapping
     * pins and the onboard WS2812. */
    const int gpio = LCD_DRIVER_BACKLIGHT_GPIO_CANDIDATE;
    const int reserved[] = {0, 3, 19, 20, 35, 36, 37, 43, 44, 45, 46, 48};
    const size_t reserved_count = sizeof(reserved) / sizeof(reserved[0]);
    const bool exposed = (gpio >= 0 && gpio <= 21) || (gpio >= 35 && gpio <= 48);

    TEST_ASSERT_TRUE(exposed);

    for (size_t i = 0; i < reserved_count; ++i) {
        TEST_ASSERT_NOT_EQUAL(reserved[i], gpio);
    }
}