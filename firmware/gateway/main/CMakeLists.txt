idf_component_register(SRCS "main.c" "nvs_config.c"
                    REQUIRES board_profile nvs_flash log lcd_driver lvgl_port ui_screens assets_brand wifi_manager
                    INCLUDE_DIRS ".")
