idf_component_register(SRCS "brand_profile.c" "brand_profile_esp.c"
                    INCLUDE_DIRS "include"
                    REQUIRES esp_common log)

# --- issue #12: brand profile resolution and brand asset budget -------------
#
# The selected profile owns a directory under profiles/ with its versioned
# assets. The build embeds the logo of that profile and refuses to produce an
# image whose brand assets do not fit the declared flash budget.

include(${CMAKE_CURRENT_LIST_DIR}/cmake/brand_profile.cmake)

brand_profile_resolve(${CMAKE_CURRENT_LIST_DIR}/profiles
                      BRAND_PROFILE_ID
                      BRAND_PROFILE_DIR)

set(BRAND_MANIFEST "${BRAND_PROFILE_DIR}/profile.manifest")
brand_profile_manifest_value("${BRAND_MANIFEST}" "logo" BRAND_LOGO_NAME)
brand_profile_manifest_value("${BRAND_MANIFEST}" "logo_width" BRAND_LOGO_WIDTH)
brand_profile_manifest_value("${BRAND_MANIFEST}" "logo_height" BRAND_LOGO_HEIGHT)
brand_profile_manifest_value("${BRAND_MANIFEST}" "asset_version" BRAND_ASSET_VERSION)

if(BRAND_LOGO_NAME STREQUAL "" OR BRAND_LOGO_WIDTH STREQUAL "" OR BRAND_LOGO_HEIGHT STREQUAL "")
    message(FATAL_ERROR
        "Brand profile '${BRAND_PROFILE_ID}' has an incomplete manifest: ${BRAND_MANIFEST}")
endif()

set(BRAND_LOGO_FILE "${BRAND_PROFILE_DIR}/${BRAND_LOGO_NAME}")
if(NOT EXISTS "${BRAND_LOGO_FILE}")
    message(FATAL_ERROR
        "Brand profile '${BRAND_PROFILE_ID}' declares the logo '${BRAND_LOGO_NAME}' "
        "but ${BRAND_LOGO_FILE} does not exist.")
endif()

file(SIZE "${BRAND_LOGO_FILE}" BRAND_LOGO_BYTES)

# The declared dimensions must match the file: RGB565 is two bytes per pixel.
math(EXPR BRAND_LOGO_EXPECTED "${BRAND_LOGO_WIDTH} * ${BRAND_LOGO_HEIGHT} * 2")
if(NOT BRAND_LOGO_BYTES EQUAL BRAND_LOGO_EXPECTED)
    message(FATAL_ERROR
        "Brand logo ${BRAND_LOGO_FILE} is ${BRAND_LOGO_BYTES} bytes but "
        "${BRAND_LOGO_WIDTH}x${BRAND_LOGO_HEIGHT} RGB565 needs ${BRAND_LOGO_EXPECTED} bytes.")
endif()

brand_profile_check_budget("${BRAND_PROFILE_ID}"
                            ${BRAND_LOGO_BYTES}
                            ${CONFIG_BRAND_ASSET_BUDGET_BYTES}
                            ${CONFIG_BRAND_FONT_RESERVE_BYTES})

brand_profile_git_revision(BRAND_BUILD_REVISION)

set(BRAND_ASSET_BUDGET_BYTES ${CONFIG_BRAND_ASSET_BUDGET_BYTES})

configure_file(${CMAKE_CURRENT_LIST_DIR}/cmake/brand_build_info.h.in
               ${CMAKE_CURRENT_BINARY_DIR}/generated/brand_build_info.h
               @ONLY)

target_include_directories(${COMPONENT_LIB} PRIVATE
                           ${CMAKE_CURRENT_BINARY_DIR}/generated)
target_compile_definitions(${COMPONENT_LIB} PRIVATE BRAND_PROFILE_EMBEDDED_LOGO=1)

# The logo is embedded with a stable symbol name, so the profile can change
# without touching the C sources.
target_add_binary_data(${COMPONENT_LIB} "${BRAND_LOGO_FILE}" BINARY
                        RENAME_TO "brand_logo")
