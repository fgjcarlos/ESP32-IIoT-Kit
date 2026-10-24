#!/usr/bin/env bash
#
# tools/brand/build-profile.sh
#
# Build the gateway with a given brand profile, in its own build directory
# and with its own sdkconfig, so two profiles can be built side by side from
# the very same sources.
#
# This is the executable form of the acceptance criterion "two example
# profiles produce firmware with a different visual identity from the same
# source code": the script changes nothing but the selected profile.
#
# Usage:
#     tools/brand/build-profile.sh iiot-kit
#     tools/brand/build-profile.sh acme-industrial
#     tools/brand/build-profile.sh acme-industrial --clean
#
# Environment:
#     IDF_EXPORT   Path to the ESP-IDF export.sh (default: $HOME/esp/esp-idf/export.sh)
#
# The public configuration of every profile lives in
# firmware/gateway/components/brand_profile/Kconfig.projbuild; the fragment
# that selects it lives in firmware/gateway/sdkconfig.brand.<profile>.

set -euo pipefail

REPO_ROOT="$(git rev-parse --show-toplevel 2>/dev/null || pwd)"
PROJECT_DIR="${REPO_ROOT}/firmware/gateway"
IDF_EXPORT="${IDF_EXPORT:-${HOME}/esp/esp-idf/export.sh}"

usage() {
    echo "usage: $(basename "$0") <iiot-kit|acme-industrial> [--clean]" >&2
    exit 2
}

PROFILE="${1:-}"
CLEAN=0
shift || true
while [[ $# -gt 0 ]]; do
    case "$1" in
        --clean) CLEAN=1 ;;
        *) usage ;;
    esac
    shift
done

[[ -n "${PROFILE}" ]] || usage

# The profile id is the lower-cased name of the Kconfig choice.
CHOICE="$(echo "${PROFILE}" | tr '[:lower:]-' '[:upper:]_')"
FRAGMENT="${PROJECT_DIR}/sdkconfig.brand.${PROFILE}"
BUILD_DIR="${PROJECT_DIR}/build-brand-${PROFILE}"

if [[ ! -f "${FRAGMENT}" ]]; then
    echo "unknown brand profile '${PROFILE}': ${FRAGMENT} does not exist" >&2
    echo "available profiles:" >&2
    ls -1 "${PROJECT_DIR}"/sdkconfig.brand.* 2>/dev/null | sed 's/.*sdkconfig.brand./  /' >&2
    exit 1
fi

if [[ "${CLEAN}" -eq 1 ]]; then
    echo "removing ${BUILD_DIR}"
    rm -rf "${BUILD_DIR}"
fi

# shellcheck disable=SC1090
# The ESP-IDF export script is not written for `set -e -u`, so the strict
# shell options are relaxed while it runs: otherwise it aborts halfway and
# the toolchain never reaches the PATH.
set +eu
source "${IDF_EXPORT}" >/dev/null 2>&1
set -eu

if ! command -v ninja >/dev/null 2>&1; then
    echo "ninja is not on the PATH: ${IDF_EXPORT} did not export the toolchain" >&2
    exit 1
fi

cd "${PROJECT_DIR}"

echo "building brand profile '${PROFILE}' (choice ${CHOICE})"
idf.py -B "${BUILD_DIR}" \
       -D SDKCONFIG="${BUILD_DIR}/sdkconfig.${PROFILE}" \
       -D SDKCONFIG_DEFAULTS="sdkconfig.defaults;sdkconfig.brand.${PROFILE}" \
       build "$@"

echo
echo "artifacts of profile '${PROFILE}':"
ls -1 "${BUILD_DIR}"/*.bin "${BUILD_DIR}"/*.manifest.json 2>/dev/null || true
