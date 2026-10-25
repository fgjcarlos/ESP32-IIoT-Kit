/*
 * PSRAM stub for the CI profile of the test application (issue #14).
 *
 * The CI runs this test project under QEMU, and the QEMU build of ESP-IDF
 * v5.4 does not emulate the Octal PSRAM of the ESP32-S3-WROOM-1-N16R8: the
 * chip aborts at boot with "Failed to init external RAM!" before the first
 * test runs. The CI profile therefore builds without PSRAM.
 *
 * The `board_profile` component links against the PSRAM API unconditionally,
 * because on the product the PSRAM of the N16R8 is part of the board contract.
 * This component provides the two symbols with the values of a board that has
 * no PSRAM, so the product code still links in the CI build. The tests that
 * assert the real PSRAM are tagged `[hw]` and run on the physical gateway.
 *
 * It is a component of its own, and it REQUIRES board_profile, because the
 * static libraries are linked in dependency order: a stub inside `main` would
 * be scanned before board_profile and the symbols would stay unresolved.
 */

#if !CONFIG_SPIRAM

#include <stdbool.h>
#include <stddef.h>

#include "esp_psram.h"

bool esp_psram_is_initialized(void)
{
    return false;
}

size_t esp_psram_get_size(void)
{
    return 0;
}

#endif /* !CONFIG_SPIRAM */
