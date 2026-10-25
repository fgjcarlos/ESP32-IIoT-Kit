idf_component_register(SRCS "test_ota_manager.c"
                    INCLUDE_DIRS "."
                    REQUIRES ota_manager ota_manifest nvs_flash
                    PRIV_REQUIRES unity)
