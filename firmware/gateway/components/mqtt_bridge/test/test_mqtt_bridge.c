#include "mqtt_bridge.h"

#include "unity.h"

TEST_CASE("mqtt_bridge contract entry points return success", "[mqtt_bridge]")
{
    TEST_ASSERT_EQUAL(ESP_OK, mqtt_bridge_start());
    TEST_ASSERT_EQUAL(ESP_OK, mqtt_bridge_stop());
}

TEST_CASE("mqtt_bridge facade is a no-op when CONFIG_SPARKPLUG_B_ENABLED is off", "[mqtt_bridge]")
{
    // The Fase 4 placeholder contract is preserved: start() and stop() always
    // return ESP_OK and log a single info message. The real implementation
    // path (CONFIG_SPARKPLUG_B_ENABLED=y) is unit-tested separately inside
    // the sparkplug_b component.
    TEST_ASSERT_EQUAL(ESP_OK, mqtt_bridge_start());
    TEST_ASSERT_EQUAL(ESP_OK, mqtt_bridge_stop());
}
