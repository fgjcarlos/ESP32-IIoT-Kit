#include "unity.h"
#include "ota_manager.h"

TEST_CASE("OTA manager contract initializes", "[ota_manager]")
{
    TEST_ASSERT_EQUAL(ESP_OK, ota_manager_init());
}