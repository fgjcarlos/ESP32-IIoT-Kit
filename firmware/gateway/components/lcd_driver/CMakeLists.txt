idf_component_register(SRCS "lcd_driver.c"
                    INCLUDE_DIRS "include"
                    REQUIRES esp_common log display_manager)