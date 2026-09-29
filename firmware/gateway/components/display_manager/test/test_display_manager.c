#include "unity.h"
#include "display_manager.h"

TEST_CASE("Display manager framebuffer constant matches documented 170x320x2",
          "[display_manager]")
{
    TEST_ASSERT_EQUAL_UINT32(170U * 320U * 2U,
                             DISPLAY_MANAGER_FRAMEBUFFER_SIZE_BYTES);
}

TEST_CASE("Display manager contract initializes", "[display_manager]")
{
    TEST_ASSERT_EQUAL(ESP_OK, display_manager_init());
}