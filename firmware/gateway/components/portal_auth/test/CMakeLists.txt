idf_component_register(SRCS "test_portal_auth.c"
                    INCLUDE_DIRS "."
                    REQUIRES portal_auth
                    PRIV_REQUIRES unity nvs_flash)
