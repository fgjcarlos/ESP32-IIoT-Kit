idf_component_register(SRCS "portal_auth.c"
                       INCLUDE_DIRS "include"
                       REQUIRES nvs_flash log mbedtls esp_common)
