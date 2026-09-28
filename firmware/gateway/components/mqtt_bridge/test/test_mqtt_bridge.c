#include "mqtt_bridge.h"

#include "unity.h"

TEST_CASE("mqtt_bridge contract entry points return success", "[mqtt_bridge]")
{
    TEST_ASSERT_EQUAL(ESP_OK, mqtt_bridge_start());
    TEST_ASSERT_EQUAL(ESP_OK, mqtt_bridge_stop());
}
