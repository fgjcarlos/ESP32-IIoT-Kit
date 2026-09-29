#pragma once

#include "esp_err.h"

/**
 * @brief RGB565 framebuffer size for the 8-pin 170x320 ST7789V2 display.
 *
 * Fixed by the documented module facts. Do not modify without a hardware
 * verification outcome (see hardware hold below).
 */
#define DISPLAY_MANAGER_FRAMEBUFFER_SIZE_BYTES (170U * 320U * 2U)

/**
 * @brief Initialize the display manager contract.
 *
 * Interface only — full implementation owned by the future display wiring
 * change, gated on physical hardware verification.
 *
 * @return ESP_OK for the contract-only placeholder.
 */
esp_err_t display_manager_init(void);

/* -------------------------------------------------------------------------
 * Hardware hold — reproduced verbatim from
 *   openspec/changes/issue-9-gateway-firmware-foundation/specs/gateway-firmware-foundation/spec.md
 *   and from CLAUDE.md / merged issue-8-gateway-board-support evidence.
 *
 * These constraints apply verbatim to every requirement that touches the
 * external 8-pin display. They MUST NOT be weakened by any future change
 * without an explicit hardware-verification outcome.
 * -------------------------------------------------------------------------
 *
 * - GPIO35, GPIO36, and GPIO37 are consumed by the N16R8 Octal PSRAM and
 *   are unavailable for external use.
 *
 * - GPIO48 is reserved for the onboard WS2812 RGB LED.
 *
 * - GPIO19 and GPIO20 are reserved for native USB while USB is needed.
 *
 * - The withdrawn seller value of `0.1155 × 0.1155 mm` display pixel pitch
 *   is incorrect and MUST NOT be used. Geometry derived from that pitch
 *   MUST NOT be inferred, and no replacement pitch SHALL be invented.
 *
 * - The 8-pin display's GPIO mapping (RES, DC, CS, SCL, SDA, BLK),
 *   backlight current/control, reset polarity, and logic input levels
 *   remain unresolved.
 *
 * - The RGB565 framebuffer size `170 × 320 × 2 = 108,800` bytes
 *   (≈106.25 KiB) is fixed by the documented module facts and SHALL be
 *   reflected as a header constant (see DISPLAY_MANAGER_FRAMEBUFFER_SIZE_BYTES
 *   above).
 */