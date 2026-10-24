cmake_minimum_required(VERSION 3.16)

set(EXTRA_COMPONENT_DIRS "components" "../common/protocol")

include($ENV{IDF_PATH}/tools/cmake/project.cmake)

# --- issue #12: brand (white-label) layer ----------------------------------
#
# The brand identity is authored once, in the brand_profile Kconfig. The
# helpers below let the build read it and let the artifact carry it.
set(BRAND_CMAKE_MODULE "${CMAKE_CURRENT_LIST_DIR}/components/brand_profile/cmake/brand_profile.cmake")
include(${BRAND_CMAKE_MODULE})

# The firmware version is part of the public configuration, and the ESP-IDF
# application descriptor is what the OTA tooling reads to know which build is
# running on a device. Copying the version into PROJECT_VER before project()
# keeps a single source of truth: the Kconfig.
#
# The value is read from the generated sdkconfig, because that is where the
# user's override lives. When the sdkconfig does not carry the key yet — it
# happens on the very first build after the option is added, because ESP-IDF
# adds the new symbol while it regenerates the file — the default declared in
# the Kconfig is used, which is exactly the value the sdkconfig is about to
# receive.
set(BRAND_VERSION "")

set(BRAND_SDKCONFIG "${CMAKE_CURRENT_LIST_DIR}/sdkconfig")
if(EXISTS "${BRAND_SDKCONFIG}")
    file(STRINGS "${BRAND_SDKCONFIG}" BRAND_VERSION_LINE
         REGEX "^CONFIG_BRAND_FIRMWARE_VERSION=")
    if(BRAND_VERSION_LINE)
        string(REGEX REPLACE "^CONFIG_BRAND_FIRMWARE_VERSION=\"([^\"]*)\"$" "\\1"
               BRAND_VERSION "${BRAND_VERSION_LINE}")
    endif()
endif()

if(NOT BRAND_VERSION)
    set(BRAND_KCONFIG "${CMAKE_CURRENT_LIST_DIR}/components/brand_profile/Kconfig.projbuild")
    file(STRINGS "${BRAND_KCONFIG}" BRAND_KCONFIG_LINES)
    set(BRAND_IN_VERSION_BLOCK FALSE)
    foreach(BRAND_KCONFIG_LINE IN LISTS BRAND_KCONFIG_LINES)
        string(STRIP "${BRAND_KCONFIG_LINE}" BRAND_KCONFIG_LINE)
        if(BRAND_KCONFIG_LINE MATCHES "^config BRAND_FIRMWARE_VERSION$")
            set(BRAND_IN_VERSION_BLOCK TRUE)
            continue()
        endif()
        if(BRAND_IN_VERSION_BLOCK)
            if(BRAND_KCONFIG_LINE MATCHES "^config ")
                set(BRAND_IN_VERSION_BLOCK FALSE)
                continue()
            endif()
            if(BRAND_KCONFIG_LINE MATCHES "^default \"([^\"]*)\"")
                set(BRAND_VERSION "${CMAKE_MATCH_1}")
                break()
            endif()
        endif()
    endforeach()
endif()

if(BRAND_VERSION)
    set(PROJECT_VER "${BRAND_VERSION}")
endif()

project(gateway)

# --- issue #12: reproducible branded artifact -------------------------------
#
# After the image is produced it is copied under the name of its brand
# profile, together with a manifest holding the metadata of the build.
if(DEFINED PROJECT_BIN)
    brand_profile_git_revision(BRAND_REVISION)
    brand_profile_resolve(${CMAKE_CURRENT_LIST_DIR}/components/brand_profile/profiles
                          BRAND_PROFILE_ID
                          BRAND_PROFILE_DIR)

    add_custom_target(brand_package ALL
        COMMAND ${CMAKE_COMMAND}
            -DBRAND_SOURCE_BIN=${CMAKE_BINARY_DIR}/${PROJECT_BIN}
            -DBRAND_CMAKE_MODULE=${BRAND_CMAKE_MODULE}
            -DBRAND_PROFILE_ID=${BRAND_PROFILE_ID}
            -DBRAND_NAME=${CONFIG_BRAND_NAME}
            -DBRAND_MODEL=${CONFIG_BRAND_MODEL}
            -DBRAND_VERSION=${CONFIG_BRAND_FIRMWARE_VERSION}
            -DBRAND_CHANNEL=${CONFIG_BRAND_OTA_CHANNEL}
            -DBRAND_REVISION=${BRAND_REVISION}
            -DBRAND_SUPPORT_URL=${CONFIG_BRAND_SUPPORT_URL}
            -DBRAND_AP_SSID=${CONFIG_BRAND_AP_SSID}
            -P ${CMAKE_CURRENT_LIST_DIR}/../../tools/brand/package.cmake
        DEPENDS gen_project_binary
        WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
        COMMENT "Naming the branded artifact"
        VERBATIM)
else()
    message(WARNING "PROJECT_BIN is not defined; the branded artifact will not be produced")
endif()
