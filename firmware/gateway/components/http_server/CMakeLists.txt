# The embedded portal asset table, generated at build time from the
# committed web/dist bundle (issue #69). Only the output path is declared
# before registration: the requirements probe runs this file in CMake
# script mode and stops at idf_component_register, so the generator rule
# itself is registered after it.
set(PORTAL_ASSETS_DATA_C ${CMAKE_CURRENT_BINARY_DIR}/generated/portal_assets_data.c)

idf_component_register(SRCS "http_server.c" "portal_api.c" "portal_assets.c"
                              ${PORTAL_ASSETS_DATA_C}
                    INCLUDE_DIRS "include"
                    REQUIRES esp_common log brand_profile esp_http_server
                             portal_auth wifi_manager esp_timer json)

find_package(Python3 COMPONENTS Interpreter REQUIRED)

set(PORTAL_DIST_DIR ${CMAKE_CURRENT_LIST_DIR}/../../web/dist)
set(PORTAL_ASSETS_SCRIPT ${CMAKE_CURRENT_LIST_DIR}/../../../../tools/web/embed_assets.py)

add_custom_command(
    OUTPUT ${PORTAL_ASSETS_DATA_C}
    COMMAND ${Python3_EXECUTABLE} ${PORTAL_ASSETS_SCRIPT}
            --dist ${PORTAL_DIST_DIR} --out ${PORTAL_ASSETS_DATA_C}
    DEPENDS ${PORTAL_DIST_DIR}/index.html
            ${PORTAL_DIST_DIR}/app.js
            ${PORTAL_DIST_DIR}/assets/index.css
            ${PORTAL_ASSETS_SCRIPT}
    COMMENT "Embedding the committed portal bundle (issue #69)"
    VERBATIM)
