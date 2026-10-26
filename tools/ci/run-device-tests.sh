#!/usr/bin/env bash
#
# Run the on-target Unity suite under QEMU and turn the log into a verdict.
#
# The CI cannot flash a board, but it does not have to: ESP-IDF v5.4 ships a
# QEMU build of the ESP32-S3, and the test application runs there. The tests
# that need the real hardware (the Octal PSRAM, the RMT of the RGB LED) are
# tagged `[hw]` and are left out by the CI profile of the application, so the
# separation between "runs everywhere" and "runs on the board" is a tag and not
# a convention.
#
# The script fails when:
#   - QEMU did not finish inside the timeout (a hang is a failure, not a pass);
#   - the Unity summary is missing (the application did not reach the end);
#   - the summary reports a failure;
#   - the application crashed after the summary (a green summary over a
#     crashed application is a failure, not a pass);
#   - the headroom of the main task is missing or below the floor.
#
# Usage:
#   tools/ci/run-device-tests.sh [project-dir] [build-dir] [timeout-seconds]

set -uo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
REPO_ROOT="$(cd "${SCRIPT_DIR}/../.." && pwd)"

PROJECT_DIR="${1:-firmware/gateway/test}"
BUILD_DIR="${2:-build/device-tests}"
TIMEOUT_SECONDS="${3:-300}"

LOG_FILE="${REPO_ROOT}/build/device-tests.log"
mkdir -p "$(dirname "${LOG_FILE}")"
# Every run starts with an empty log, so the summary of this run cannot be
# confused with the summary of a previous one.
rm -f "${LOG_FILE}" "${LOG_FILE}.build"

cd "${REPO_ROOT}"

if ! command -v idf.py >/dev/null 2>&1; then
    echo "run-device-tests: idf.py is not on PATH; run 'source \$HOME/esp/esp-idf/export.sh' first" >&2
    exit 1
fi

# QEMU is an optional tool of ESP-IDF, and the tool manager installs it in a
# directory that the environment of the shell does not know about: the
# environment was exported before the tool was installed. The binary is
# looked up where the tool manager puts it.
if ! command -v qemu-system-xtensa >/dev/null 2>&1; then
    qemu_bin="$(find "${HOME}/.espressif/tools" -name qemu-system-xtensa -type f 2>/dev/null | head -1)"
    if [[ -z "${qemu_bin}" ]]; then
        echo "run-device-tests: qemu-system-xtensa is not installed; run" >&2
        echo "  python \$IDF_PATH/tools/idf_tools.py install qemu-xtensa" >&2
        exit 1
    fi
    export PATH="$(dirname "${qemu_bin}"):${PATH}"
fi

echo "run-device-tests: building the CI profile of the test application" >&2
# The generated configuration is removed first, so the profile always starts
# from sdkconfig.defaults + sdkconfig.qemu.defaults and a stale file cannot
# carry an old option into the run.
rm -f "${REPO_ROOT}/${PROJECT_DIR}/sdkconfig.qemu"

if ! idf.py -C "${PROJECT_DIR}" -B "${BUILD_DIR}" \
        -D SDKCONFIG=sdkconfig.qemu \
        -D "SDKCONFIG_DEFAULTS=sdkconfig.defaults;sdkconfig.qemu.defaults" \
        build > "${LOG_FILE}.build" 2>&1; then
    echo "run-device-tests: the build of the test application failed" >&2
    tail -30 "${LOG_FILE}.build" >&2
    exit 1
fi

echo "run-device-tests: running the suite under QEMU (timeout ${TIMEOUT_SECONDS}s)" >&2
# -no-reboot stops the chip when the application returns from app_main: without
# it the bootloader restarts the application and the suite runs again and
# again until the timeout.
timeout "${TIMEOUT_SECONDS}" idf.py -C "${PROJECT_DIR}" -B "${BUILD_DIR}" \
    qemu --qemu-extra-args "-no-reboot" \
    > "${LOG_FILE}" 2>&1
qemu_rc=$?

if [[ ${qemu_rc} -eq 124 ]]; then
    echo "run-device-tests: QEMU did not finish within ${TIMEOUT_SECONDS}s" >&2
    tail -40 "${LOG_FILE}" >&2
    exit 1
fi

# The log comes from a serial line, so every line ends with a carriage
# return. The parser works on a copy without them.
CLEAN_LOG="${LOG_FILE}.clean"
tr -d '\r' < "${LOG_FILE}" > "${CLEAN_LOG}"

summary="$(grep -a -E '^[0-9]+ Tests [0-9]+ Failures [0-9]+ Ignored' "${CLEAN_LOG}" | tail -1)"

if [[ -z "${summary}" ]]; then
    echo "run-device-tests: the Unity summary is missing; the application did not finish" >&2
    tail -40 "${CLEAN_LOG}" >&2
    exit 1
fi

# The Unity summary is "N Tests M Failures K Ignored": the number of failures
# is the one that follows the word "Failures".
failures="$(echo "${summary}" | awk '{for (i = 1; i <= NF; i++) if ($i == "Failures") print $(i + 1)}')"
verdict="$(grep -a -E '^(OK|FAIL)$' "${CLEAN_LOG}" | tail -1)"

if [[ "${failures}" != "0" || "${verdict}" != "OK" ]]; then
    echo "run-device-tests: the suite FAILED (${summary}, verdict '${verdict}')" >&2
    grep -a -E ':FAIL' "${CLEAN_LOG}" | head -20 >&2
    exit 1
fi

# A green summary is not a green run.
#
# The suite can pass every test and still leave the application crashing: the
# Unity summary is printed before the FreeRTOS stack overflow detector fires,
# so a run that ends with an overflow looks green to the checks above. That is
# exactly what happened with the 8192-byte main task stack: "114 Tests 0
# Failures" followed by "A stack overflow in task main has been detected" and
# a reboot.
#
# A crash after the summary is a failure of the run, so it is checked here.
crash_lines="$(grep -a -n -E '\*\*\*ERROR\*\*\*|stack overflow|Guru Meditation|abort\(\) was called|Rebooting\.\.\.|rst:0x[0-9a-f]+ \(.*(PANIC|panic)' "${CLEAN_LOG}" | head -10)"

if [[ -n "${crash_lines}" ]]; then
    echo "run-device-tests: the application crashed after the summary; the run is a failure" >&2
    echo "${crash_lines}" >&2
    exit 1
fi

# The headroom of the main task is the early warning of the overflow above.
# The application measures it at the end of the run and prints it; a run that
# leaves less than the floor is a run that is one test away from an overflow,
# and it is cheaper to catch it here than in the crash report.
#
# The floor is 1024 bytes. The measured value with the 114 tests of the suite
# is 7504 bytes of 16384, so the floor leaves room for the suite to grow
# before it has to be a deliberate change.
HEADROOM_FLOOR_BYTES="${HEADROOM_FLOOR_BYTES:-1024}"

headroom_line="$(grep -a -E 'Main task stack headroom after the run: [0-9]+ bytes of [0-9]+' "${CLEAN_LOG}" | tail -1)"

if [[ -z "${headroom_line}" ]]; then
    echo "run-device-tests: the main task stack headroom was not measured; the run is a failure" >&2
    tail -40 "${CLEAN_LOG}" >&2
    exit 1
fi

headroom="$(echo "${headroom_line}" | sed -E 's/.*headroom after the run: ([0-9]+) bytes.*/\1/')"

if [[ "${headroom}" -lt "${HEADROOM_FLOOR_BYTES}" ]]; then
    echo "run-device-tests: the main task stack headroom is ${headroom} bytes, below the floor of ${HEADROOM_FLOOR_BYTES}" >&2
    exit 1
fi

echo
echo "## Device tests under QEMU"
echo
echo '```'
echo "${summary}"
echo "${verdict}"
echo "${headroom_line}"
echo '```'
echo
echo "The tests tagged \`[hw]\` are excluded from this run: they need the physical gateway."

exit 0
