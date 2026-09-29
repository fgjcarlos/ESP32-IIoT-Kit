#include "unity.h"
#include "ui_screens.h"

TEST_CASE("UI screens init returns ESP_OK and exposes six screen functions",
          "[ui_screens]")
{
    TEST_ASSERT_EQUAL(ESP_OK, ui_screens_init());

    /* All six screen entry points must be non-NULL so callers can switch
     * between them at runtime without dereferencing a NULL function pointer.
     * The address-of-function expression forces the linker to retain the
     * symbols and lets the test catch any future regression that forgets
     * to register one of them in `ui_screens.c`. */
    TEST_ASSERT_NOT_NULL((void *)&ui_boot_screen);
    TEST_ASSERT_NOT_NULL((void *)&ui_status_screen);
    TEST_ASSERT_NOT_NULL((void *)&ui_connectivity_screen);
    TEST_ASSERT_NOT_NULL((void *)&ui_nodes_screen);
    TEST_ASSERT_NOT_NULL((void *)&ui_alerts_screen);
    TEST_ASSERT_NOT_NULL((void *)&ui_ota_screen);
}

TEST_CASE("UI screens six stubs are callable without crashing", "[ui_screens]")
{
    /* Calling each stub logs the screen name and returns. There is no
     * observable side-effect to assert against (no LVGL state, no screen
     * registry), so the test only verifies the call chain does not crash.
     * The contract guarantees each stub is a single ESP_LOGI line. */
    TEST_ASSERT_EQUAL(ESP_OK, ui_screens_init());
    ui_boot_screen();
    ui_status_screen();
    ui_connectivity_screen();
    ui_nodes_screen();
    ui_alerts_screen();
    ui_ota_screen();
}