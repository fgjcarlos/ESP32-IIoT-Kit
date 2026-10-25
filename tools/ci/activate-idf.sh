#!/usr/bin/env bash
#
# Activate the ESP-IDF environment.
#
# The CI checks ESP-IDF out next to the repository, so every step that builds
# something sources this file first. It works in the CI (IDF_PATH comes from
# the GITHUB_ENV of the install step) and on a developer machine (the usual
# ~/esp/esp-idf).
#
# Usage:
#   . tools/ci/activate-idf.sh

if [[ -n "${IDF_PATH:-}" && -f "${IDF_PATH}/export.sh" ]]; then
    export_script="${IDF_PATH}/export.sh"
elif [[ -f "${GITHUB_WORKSPACE:-}/esp-idf/export.sh" ]]; then
    export_script="${GITHUB_WORKSPACE}/esp-idf/export.sh"
elif [[ -f "${HOME}/esp/esp-idf/export.sh" ]]; then
    export_script="${HOME}/esp/esp-idf/export.sh"
else
    echo "activate-idf: no ESP-IDF checkout found (IDF_PATH, \$GITHUB_WORKSPACE/esp-idf or ~/esp/esp-idf)" >&2
    return 1 2>/dev/null || exit 1
fi

# shellcheck disable=SC1090
. "${export_script}" >/dev/null

if ! command -v idf.py >/dev/null 2>&1; then
    echo "activate-idf: ${export_script} did not put idf.py on PATH" >&2
    return 1 2>/dev/null || exit 1
fi
