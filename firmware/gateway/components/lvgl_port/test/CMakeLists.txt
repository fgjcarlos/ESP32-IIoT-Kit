idf_component_register(SRCS "test_lvgl_port.c"
                    INCLUDE_DIRS "."
                    REQUIRES lvgl_port
                    PRIV_REQUIRES unity)