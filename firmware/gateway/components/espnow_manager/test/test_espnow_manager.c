#include "unity.h"
#include "espnow_manager.h"

TEST_CASE("ESP-NOW manager contract initializes", "[espnow_manager]")
{
    TEST_ASSERT_EQUAL(ESP_OK, espnow_manager_init());
}
