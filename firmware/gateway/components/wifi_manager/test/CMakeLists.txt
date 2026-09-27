idf_component_register(SRCS "test_wifi_manager.c"
                    INCLUDE_DIRS "."
                    REQUIRES wifi_manager
                    PRIV_REQUIRES unity)
