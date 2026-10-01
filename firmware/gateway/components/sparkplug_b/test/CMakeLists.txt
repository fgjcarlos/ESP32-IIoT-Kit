idf_component_register(SRCS "test_payload.c" "test_state.c"
                    INCLUDE_DIRS "."
                    REQUIRES sparkplug_b unity nvs_flash)
