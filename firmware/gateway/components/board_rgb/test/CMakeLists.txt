idf_component_register(SRCS "test_board_rgb.c"
                    INCLUDE_DIRS "."
                    REQUIRES board_rgb freertos
                    PRIV_REQUIRES unity)
