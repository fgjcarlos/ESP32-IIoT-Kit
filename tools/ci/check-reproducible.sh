#!/usr/bin/env bash
#
# Build the firmware twice from a clean state and compare the artefacts.
#
# A build is reproducible when the same sources produce the same bytes. The
# gateway enables `CONFIG_APP_REPRODUCIBLE_BUILD` (sdkconfig.defaults), which
# removes the compile date, the compile time and the absolute build paths from
# the image, so two builds of the same commit must be identical file by file.
#
# The check is deliberately mechanical: it builds into two separate build
# directories, hashes the three artefacts that make up a flashable image and
# fails when any of them differs.
#
# Usage:
#   tools/ci/check-reproducible.sh [project-dir] [build-root]
#
# The ESP-IDF environment must be loaded first:
#   source $HOME/esp/esp-idf/export.sh

set -euo pipefail

PROJECT_DIR="${1:-firmware/gateway}"
BUILD_ROOT="${2:-build/reproducible}"

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
REPO_ROOT="$(cd "${SCRIPT_DIR}/../.." && pwd)"

cd "${REPO_ROOT}"

if ! command -v idf.py >/dev/null 2>&1; then
    echo "check-reproducible: idf.py is not on PATH; run 'source \$HOME/esp/esp-idf/export.sh' first" >&2
    exit 1
fi

ARTIFACTS=(
    "gateway.bin"
    "bootloader/bootloader.bin"
    "partition_table/partition-table.bin"
)

BUILD_A="${BUILD_ROOT}/a"
BUILD_B="${BUILD_ROOT}/b"

echo "check-reproducible: building ${PROJECT_DIR} twice" >&2
rm -rf "${BUILD_A}" "${BUILD_B}"

idf.py -B "${BUILD_A}" -C "${PROJECT_DIR}" build >/dev/null
idf.py -B "${BUILD_B}" -C "${PROJECT_DIR}" build >/dev/null

status=0
rows=""

for artifact in "${ARTIFACTS[@]}"; do
    file_a="${BUILD_A}/${artifact}"
    file_b="${BUILD_B}/${artifact}"

    if [[ ! -f "${file_a}" || ! -f "${file_b}" ]]; then
        echo "check-reproducible: missing artefact ${artifact}" >&2
        status=1
        continue
    fi

    hash_a="$(sha256sum "${file_a}" | cut -d' ' -f1)"
    hash_b="$(sha256sum "${file_b}" | cut -d' ' -f1)"

    if [[ "${hash_a}" == "${hash_b}" ]]; then
        verdict="identical"
    else
        verdict="**DIFFERENT**"
        status=1
    fi

    rows+="| \`${artifact}\` | \`${hash_a:0:16}\` | \`${hash_b:0:16}\` | ${verdict} |"$'\n'
done

cat <<EOF
## Reproducible build

| Artefact | Build A | Build B | Verdict |
|---|---|---|---|
${rows}
EOF

if [[ ${status} -ne 0 ]]; then
    echo "check-reproducible: the two builds are not identical" >&2
fi

exit ${status}
