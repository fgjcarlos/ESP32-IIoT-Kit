# Host-side test build for the gateway's portable logic.
#
# Builds the real component sources together with the same Unity test files
# the on-device test project runs, so a change that breaks a test breaks it
# in both places.
#
# Two binaries are produced:
#   iot_host_tests  the Unity suite, on the host
#   iot_stress      the stress harness with N synthetic nodes (issue #14)
#
# Requirements: a C compiler, CMake, and an ESP-IDF checkout (only for the
# portable Unity core and the Linux port). No ESP-IDF environment is needed:
#   cmake -S tools/host-tests -B build/host-tests && cmake --build build/host-tests
#   ./build/host-tests/iot_host_tests
#   ./build/host-tests/iot_stress --nodes 10 --rounds 20

cmake_minimum_required(VERSION 3.16)
project(iot_host_tests C)

set(REPO_ROOT ${CMAKE_CURRENT_SOURCE_DIR}/../..)

# ESP-IDF is used for its portable Unity core and its Linux port. The path can
# be overridden with -DIDF_ROOT=... when the checkout is not in the default
# location.
if(NOT IDF_ROOT)
    if(DEFINED ENV{IDF_PATH})
        set(IDF_ROOT $ENV{IDF_PATH})
    else()
        set(IDF_ROOT $ENV{HOME}/esp/esp-idf)
    endif()
endif()

set(UNITY_DIR ${IDF_ROOT}/components/unity)
set(NANOPB_DIR ${REPO_ROOT}/firmware/gateway/managed_components/livekit__nanopb)
set(SPARKPLUG_DIR ${REPO_ROOT}/firmware/gateway/components/sparkplug_b)
set(BRAND_DIR ${REPO_ROOT}/firmware/gateway/components/brand_profile)
set(OTA_MANIFEST_DIR ${REPO_ROOT}/firmware/gateway/components/ota_manifest)

foreach(required ${UNITY_DIR} ${NANOPB_DIR} ${SPARKPLUG_DIR} ${BRAND_DIR} ${OTA_MANIFEST_DIR})
    if(NOT EXISTS ${required})
        message(FATAL_ERROR "Missing ${required}. Pass -DIDF_ROOT=<esp-idf checkout> if it is not in the default location.")
    endif()
endforeach()

# The portable code under test: the components of the gateway that do not
# need an ESP-IDF runtime. Both binaries link against it.
add_library(iot_portable OBJECT
    shims/shims.c

    # nanopb, vendored by the component.
    ${NANOPB_DIR}/src/pb_common.c
    ${NANOPB_DIR}/src/pb_decode.c
    ${NANOPB_DIR}/src/pb_encode.c

    # The code under test.
    ${REPO_ROOT}/firmware/common/protocol/protocol.c
    ${SPARKPLUG_DIR}/src/sparkplug_b_payload.c
    ${SPARKPLUG_DIR}/src/sparkplug_b_cmd.c
    ${SPARKPLUG_DIR}/src/sparkplug_b_device.c
    ${SPARKPLUG_DIR}/src/sparkplug_b_state.c
    ${SPARKPLUG_DIR}/src/sparkplug_b_topics.c
    ${SPARKPLUG_DIR}/src/sparkplug_b_espnow_bridge.c
    ${SPARKPLUG_DIR}/proto/sparkplug_b.pb.c

    # The white-label layer: the portable rules and the ESP-IDF glue.
    ${BRAND_DIR}/brand_profile.c
    ${BRAND_DIR}/brand_profile_esp.c

    # The OTA release manifest: the portable rules that decide whether an
    # image may be installed.
    ${OTA_MANIFEST_DIR}/ota_manifest.c
)

target_include_directories(iot_portable PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/shims
    ${NANOPB_DIR}/include
    ${REPO_ROOT}/firmware/common/protocol
    ${SPARKPLUG_DIR}/include
    ${SPARKPLUG_DIR}/proto
    ${BRAND_DIR}/include
    ${OTA_MANIFEST_DIR}/include
)

# The device build embeds the logo of the selected profile; the host build
# takes the same code path with the placeholder declared in shims/.
target_compile_definitions(iot_portable PUBLIC BRAND_PROFILE_EMBEDDED_LOGO=1)

target_compile_options(iot_portable PRIVATE -Wall -Wextra -Wno-unused-parameter)

add_executable(iot_host_tests
    main.c

    # Portable Unity core plus the ESP-IDF runner and its Linux port.
    ${UNITY_DIR}/unity/src/unity.c
    ${UNITY_DIR}/unity_runner.c
    ${UNITY_DIR}/unity_port_linux.c
    ${UNITY_DIR}/unity_compat.c

    # The same test files the on-device test project runs.
    ${REPO_ROOT}/firmware/common/protocol/test/test_protocol.c
    ${SPARKPLUG_DIR}/test/test_payload.c
    ${SPARKPLUG_DIR}/test/test_state.c
    ${SPARKPLUG_DIR}/test/test_nvs.c
    ${SPARKPLUG_DIR}/test/test_espnow_bridge.c
    ${SPARKPLUG_DIR}/test/test_cmd.c
    ${SPARKPLUG_DIR}/test/test_lifecycle.c
    ${BRAND_DIR}/test/test_brand_profile.c
    ${OTA_MANIFEST_DIR}/test/test_ota_manifest.c
)

# Linking the object library propagates its include directories and its
# definitions to the consumers.
target_link_libraries(iot_host_tests PRIVATE iot_portable)

target_include_directories(iot_host_tests PRIVATE
    ${UNITY_DIR}/include
    ${UNITY_DIR}/unity/src
)

target_compile_options(iot_host_tests PRIVATE -Wall -Wextra -Wno-unused-parameter)

# The ESP-IDF Unity component is built with this define; it pulls in
# unity_config.h, which in turn provides the TEST_CASE registry.
target_compile_definitions(iot_host_tests PRIVATE UNITY_INCLUDE_CONFIG_H)

# The stress harness: N synthetic nodes through the real bridge.
add_executable(iot_stress
    stress_nodes.c
)

target_link_libraries(iot_stress PRIVATE iot_portable)

target_compile_options(iot_stress PRIVATE -Wall -Wextra -Wno-unused-parameter)
