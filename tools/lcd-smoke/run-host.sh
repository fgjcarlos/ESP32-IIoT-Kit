#!/usr/bin/env bash
#
# Host test runner for the ST7789 diagnostic pattern.
#
# It reuses the repository's existing host harness (tools/host-tests/main.c and
# its shims) together with the Unity core of the installed ESP-IDF checkout,
# so the pattern is verified on the host, without a board and without touching
# the shared gateway test project.
#
# Usage:
#   tools/lcd-smoke/run-host.sh
#
# Requirements: a C compiler and an ESP-IDF checkout. The checkout is taken
# from IDF_ROOT, then from IDF_PATH, then from $HOME/esp/esp-idf.

set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
REPO_ROOT="$(cd "${SCRIPT_DIR}/../.." && pwd)"
IDF_ROOT="${IDF_ROOT:-${IDF_PATH:-${HOME}/esp/esp-idf}}"
UNITY_DIR="${IDF_ROOT}/components/unity"

if [ ! -d "${UNITY_DIR}" ]; then
    echo "ESP-IDF checkout not found at ${IDF_ROOT}: set IDF_PATH or IDF_ROOT." >&2
    exit 1
fi

# The scratch build lives outside the repository and is removed on exit.
BUILD_DIR="$(mktemp -d)"
trap 'rm -rf "${BUILD_DIR}"' EXIT

CC_BIN="${CC:-cc}"
INCLUDES=(
    -I"${REPO_ROOT}/tools/host-tests/shims"
    -I"${SCRIPT_DIR}/components/lcd_smoke_pattern/include"
    -I"${UNITY_DIR}/include"
    -I"${UNITY_DIR}/unity/src"
)

# The component under test and its test file are compiled as strict C11 with
# -Werror: a warning there is a failure of this gate.
"${CC_BIN}" -std=c11 -Wall -Wextra -Werror -DUNITY_INCLUDE_CONFIG_H "${INCLUDES[@]}" \
    -c "${SCRIPT_DIR}/components/lcd_smoke_pattern/lcd_smoke_pattern.c" \
    -o "${BUILD_DIR}/lcd_smoke_pattern.o"
"${CC_BIN}" -std=c11 -Wall -Wextra -Werror -DUNITY_INCLUDE_CONFIG_H "${INCLUDES[@]}" \
    -c "${SCRIPT_DIR}/components/lcd_smoke_pattern/test/test_lcd_smoke_pattern.c" \
    -o "${BUILD_DIR}/test_lcd_smoke_pattern.o"

# The shared harness and the Unity core of ESP-IDF are compiled as they come:
# they are POSIX code, not C11 code of this change.
for source in \
    "${REPO_ROOT}/tools/host-tests/main.c" \
    "${REPO_ROOT}/tools/host-tests/shims/shims.c" \
    "${UNITY_DIR}/unity/src/unity.c" \
    "${UNITY_DIR}/unity_runner.c" \
    "${UNITY_DIR}/unity_port_linux.c" \
    "${UNITY_DIR}/unity_compat.c"
do
    "${CC_BIN}" -std=gnu11 -Wall -Wextra -Wno-unused-parameter -DUNITY_INCLUDE_CONFIG_H "${INCLUDES[@]}" \
        -c "${source}" -o "${BUILD_DIR}/$(basename "${source}" .c).o"
done

"${CC_BIN}" -o "${BUILD_DIR}/lcd_smoke_host_tests" "${BUILD_DIR}"/*.o
"${BUILD_DIR}/lcd_smoke_host_tests"
