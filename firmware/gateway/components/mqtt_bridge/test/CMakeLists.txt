idf_component_register(SRCS "test_mqtt_bridge.c"
                    INCLUDE_DIRS "."
                    REQUIRES mqtt_bridge
                    PRIV_REQUIRES unity)
