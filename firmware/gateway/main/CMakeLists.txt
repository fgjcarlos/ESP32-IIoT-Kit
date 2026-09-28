idf_component_register(SRCS "main.c" "nvs_config.c"
                    REQUIRES board_profile nvs_flash log
                    INCLUDE_DIRS ".")
