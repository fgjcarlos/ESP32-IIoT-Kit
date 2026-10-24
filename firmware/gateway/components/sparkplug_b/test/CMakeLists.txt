idf_component_register(SRCS "test_payload.c" "test_state.c" "test_nvs.c" "test_espnow_bridge.c" "test_cmd.c"
                    INCLUDE_DIRS "."
                    REQUIRES sparkplug_b unity nvs_flash protocol)
