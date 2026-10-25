#!/usr/bin/env bash
# Build a *defective* firmware image, to demonstrate the rollback of issue #13.
#
# The image is a copy of the gateway firmware whose `app_main` does nothing
# but restart the unit over and over. It is a perfectly valid ESP32-S3
# application image: the bootloader accepts it and starts it. What it never
# does is confirm itself, which is exactly the situation the rollback
# exists for.
#
# The copy lives outside the repository: the sources of the project are not
# touched.
#
# Usage:
#   tools/ota/make-defective-image.sh [output-directory]
#
# The result is `<output-directory>/gateway.bin`, ready to be written into
# the inactive slot:
#
#   esptool.py --chip esp32s3 --port /dev/ttyACM0 write_flash 0x320000 \
#              gateway.bin

set -euo pipefail

REPO_ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
OUTPUT_DIR="${1:-${REPO_ROOT}/build/ota-defective}"

if [[ -z "${IDF_PATH:-}" ]]; then
    echo "the ESP-IDF environment is not loaded: source \$HOME/esp/esp-idf/export.sh" >&2
    exit 1
fi

WORKDIR="$(mktemp -d /tmp/ota-defective-XXXXXX)"

cleanup() {
    # The shell may still be inside the directory, so leave it before it is
    # removed.
    cd "${REPO_ROOT}" || true
    rm -rf "${WORKDIR}" 2>/dev/null || true
}
trap cleanup EXIT

# The copy keeps the layout of the repository: the project refers to the
# shared protocol component and to the packaging tooling with relative
# paths.
PROJECT="${WORKDIR}/firmware/gateway"
mkdir -p "${PROJECT}" "${WORKDIR}/firmware/common" "${WORKDIR}/tools"

# Copy the sources without the build artefacts.
tar -C "${REPO_ROOT}/firmware/gateway" \
    --exclude=build --exclude='build-*' --exclude=managed_components \
    -cf - . | tar -C "${PROJECT}" -xf -
tar -C "${REPO_ROOT}/firmware/common" \
    --exclude=build --exclude='build-*' --exclude=managed_components \
    -cf - . | tar -C "${WORKDIR}/firmware/common" -xf -
tar -C "${REPO_ROOT}/tools" \
    --exclude=build --exclude='build-*' --exclude='__pycache__' \
    -cf - . | tar -C "${WORKDIR}/tools" -xf -

# The defective application: it starts, it says so, and it restarts.
cat > "${PROJECT}/main/main.c" <<'EOF'
#include <stdbool.h>

#include "esp_log.h"
#include "esp_system.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

static const char *TAG = "ota_defective";

/*
 * This image is the demonstration of the rollback: it boots, it never
 * reaches a working state and it never confirms itself, so the bootloader
 * has to discard it and go back to the previous slot.
 *
 * The panic is deliberate: it is what a defective image does, and the
 * backtrace is the evidence that the unit really ran it.
 */
void app_main(void)
{
    ESP_LOGE(TAG, "this image is defective: it never confirms itself");
    ESP_LOGE(TAG, "the bootloader must roll back to the previous slot");

    vTaskDelay(pdMS_TO_TICKS(1500));
    abort();
}
EOF

mkdir -p "${OUTPUT_DIR}"
cd "${PROJECT}"
idf.py build > "${WORKDIR}/build.log" 2>&1 || {
    echo "the build of the defective image failed:" >&2
    tail -30 "${WORKDIR}/build.log" >&2
    exit 1
}

cp "${PROJECT}/build/gateway.bin" "${OUTPUT_DIR}/gateway.bin"

SIZE=$(stat -c %s "${OUTPUT_DIR}/gateway.bin")
echo "the defective image was written to ${OUTPUT_DIR}/gateway.bin (${SIZE} bytes)"
echo
echo "write it into the inactive slot and arm it:"
echo "  esptool.py --chip esp32s3 --port /dev/ttyACM0 write_flash 0x320000 \\"
echo "             ${OUTPUT_DIR}/gateway.bin"
echo "  tools/ota/make-otadata.py --slot ota_1 --out /tmp/otadata.bin"
echo "  esptool.py --chip esp32s3 --port /dev/ttyACM0 write_flash 0xf000 \\"
echo "             /tmp/otadata.bin"
