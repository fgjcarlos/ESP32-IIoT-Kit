idf_component_register(SRCS "ota_manager.c"
                    INCLUDE_DIRS "include"
                    REQUIRES esp_common esp_http_client esp_partition
                             app_update mbedtls esp-tls nvs_flash
                             ota_manifest brand_profile board_profile)

# The public half of the release signing key. The firmware needs it to
# verify the manifests of the releases it is allowed to install. A public
# key is not a secret: the private half lives in the release pipeline and
# never reaches the repository.
target_add_binary_data(${COMPONENT_LIB}
                        "${CMAKE_CURRENT_SOURCE_DIR}/certs/ota_signing_key.pem"
                        TEXT)
