#pragma once

#include "esp_err.h"

/**
 * @brief Initialize the LCD driver contract.
 *
 * Interface only — full implementation is owned by the future display wiring
 * change, gated on physical hardware verification.
 *
 * @return ESP_OK for the contract-only placeholder.
 *
 * @note The contract does NOT call `esp_lcd_*`, does NOT initialize any SPI
 *       bus, does NOT assign any GPIO, does NOT power on the panel, and
 *       does NOT define a draw callback. Real panel init/reset/set_window/
 *       draw belongs in a later Fase and is gated on physical carrier
 *       verification.
 *
 * @note The backlight control GPIO is documented as a candidate (see
 *       LCD_DRIVER_BACKLIGHT_GPIO_CANDIDATE below) and is **NOT** a binding
 *       assignment. The value stays pending physical verification.
 */
esp_err_t lcd_driver_init(void);

/**
 * @brief Backlight control GPIO candidate.
 *
 * Documented as candidate, **NOT** a binding assignment. The actual
 * carrier-to-display wiring (BLK contact on the 8-pin module) is pending
 * physical verification. Treat the value here as a design-time proposal
 * that must be confirmed by reading the carrier pins before any production
 * wiring decision.
 *
 * Defined here (header) rather than in the .c so future wiring code can
 * `#include` it and reference a single source of truth for the candidate.
 */
#define LCD_DRIVER_BACKLIGHT_GPIO_CANDIDATE 22

/* -------------------------------------------------------------------------
 * Display contact map — confirmed by user on issue #10.
 *
 * The selected external panel is an 8-pin ST7789V2 module with 4-wire SPI,
 * 170(H) RGB × 320(V), nominal 1.9-inch. The contact numbers are the
 * physical module pin numbers (1..8), NOT ESP32 GPIO assignments. The
 * GPIO mapping for these contacts is pending physical carrier verification
 * and is intentionally NOT specified here.
 *
 *   1  GND    — ground
 *   2  VCC    — supply (3.3 V nominal; logic input levels remain unverified)
 *   3  SCL    — SPI clock (display contact)
 *   4  SDA    — SPI data input (display contact; MOSI relative to ESP32)
 *   5  RES    — reset (display contact; polarity unverified)
 *   6  DC     — data/command select (display contact)
 *   7  CS     — chip select (display contact)
 *   8  BLK    — backlight (display contact; GPIO22 candidate, pending
 *               physical verification — see LCD_DRIVER_BACKLIGHT_GPIO_CANDIDATE)
 *
 * The framebuffer constant lives in `display_manager.h`
 * (DISPLAY_MANAGER_FRAMEBUFFER_SIZE_BYTES = 170 × 320 × 2 = 108,800 bytes).
 * Issue #10's body said `172×320`; the correct resolution is `170×320`.
 *
 * SPI bus sharing (LCD + future TF card) is documented as a constraint on
 * every contract introduced for issue #10. No bus arbitration code lives in
 * this contract; the TF card driver is out of scope.
 * ------------------------------------------------------------------------- */