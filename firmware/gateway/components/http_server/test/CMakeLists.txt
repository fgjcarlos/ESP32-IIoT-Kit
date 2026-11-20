idf_component_register(SRCS "test_http_server.c" "test_portal_assets.c"
                    INCLUDE_DIRS "."
                    REQUIRES http_server
                    PRIV_REQUIRES unity)
