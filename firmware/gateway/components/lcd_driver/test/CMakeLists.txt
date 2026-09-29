idf_component_register(SRCS "test_lcd_driver.c"
                    INCLUDE_DIRS "."
                    REQUIRES lcd_driver
                    PRIV_REQUIRES unity)