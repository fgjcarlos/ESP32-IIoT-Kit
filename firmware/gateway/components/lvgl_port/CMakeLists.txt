idf_component_register(SRCS "lvgl_port.c"
                    INCLUDE_DIRS "include"
                    REQUIRES esp_common log display_manager lcd_driver)