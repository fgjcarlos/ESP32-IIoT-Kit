# mqtt_bridge is a thin facade (commit 3) that delegates to sparkplug_b
# when the feature flag is enabled. The sparkplug_b component is always
# available; the flag only controls runtime behaviour inside mqtt_bridge.c
# via `#if CONFIG_SPARKPLUG_B_ENABLED`. Linker dead-strip takes care of
# removing the unused branch in production builds.

idf_component_register(SRCS "mqtt_bridge.c"
                    INCLUDE_DIRS "include"
                    REQUIRES esp_common log sparkplug_b)