idf_component_register(SRCS "board_rgb.c"
                    INCLUDE_DIRS "include"
                    REQUIRES esp_common esp_driver_rmt freertos log)
