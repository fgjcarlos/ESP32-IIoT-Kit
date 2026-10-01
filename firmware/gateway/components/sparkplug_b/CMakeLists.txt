# Sparkplug B component.
# - Commit 1: stub + Kconfig + sdkconfig pin
# - Commit 3: header API stub (mqtt_bridge delegates here)
# - Commit 4+: .proto + nanopb codegen + state machine + MQTT client
# SRCS stays empty until commit 4 lands the real implementation.
idf_component_register(SRCS ""
                    INCLUDE_DIRS "include"
                    REQUIRES "")
