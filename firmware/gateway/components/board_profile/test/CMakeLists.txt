idf_component_register(SRCS "test_board_profile.c"
                    INCLUDE_DIRS "."
                    REQUIRES board_profile heap log
                    PRIV_REQUIRES unity)
