idf_component_register(SRCS "test_espnow_manager.c"
                    INCLUDE_DIRS "."
                    REQUIRES espnow_manager
                    PRIV_REQUIRES unity)
