#pragma once

#include "esp_err.h"

/**
 * @brief Initialize the UI screens orchestrator contract.
 *
 * Interface only — full implementation is owned by the future display wiring
 * change, gated on physical hardware verification.
 *
 * @return ESP_OK for the contract-only placeholder.
 *
 * @note The contract does NOT call any `lv_*` function, does NOT register
 *       LVGL screens, does NOT create widgets, and does NOT bind UI events.
 *       Real LVGL screen bring-up belongs in a later Fase and is gated on
 *       `lvgl_port` wiring plus physical carrier verification.
 *
 * @note The six screen entry points declared in this header are documented
 *       as the future show targets for the LVGL UI. Each one is a contract
 *       stub in this slice; calling them logs the screen name and returns.
 *       They are intentionally minimal so a future Fase can implement them
 *       without changing the public API.
 */
esp_err_t ui_screens_init(void);

/* -------------------------------------------------------------------------
 * Six LVGL screen entry points — contract stubs.
 *
 * Each function is a future `void ui_<name>_show(void)` target. The contract
 * in this slice logs the screen name and returns; the future implementation
 * will switch the active LVGL screen to the named view.
 *
 * The six screens cover the local UX listed in issue #10:
 *
 *   1. ui_boot_screen         — first-screen splash shown at boot.
 *   2. ui_status_screen       — gateway status (heap, PSRAM, uptime).
 *   3. ui_connectivity_screen — WiFi AP/STA + ESP-NOW channel state.
 *   4. ui_nodes_screen        — list of discovered sensor nodes.
 *   5. ui_alerts_screen       — recent alerts / OTA failure / NVS errors.
 *   6. ui_ota_screen          — OTA progress and slot state.
 *
 * All six functions take no arguments and return void; they are deliberately
 * side-effect-only at the contract level (one log line each).
 * ------------------------------------------------------------------------- */
void ui_boot_screen(void);
void ui_status_screen(void);
void ui_connectivity_screen(void);
void ui_nodes_screen(void);
void ui_alerts_screen(void);
void ui_ota_screen(void);

/* -------------------------------------------------------------------------
 * SPI bus shared constraint — LCD + future TF card.
 *
 * Mirrors the note in `lvgl_port.h`: the display contract chain shares the
 * SPI bus with the TF card driver that will be introduced in a later Fase.
 * No bus arbitration code lives in this contract. UI screens do NOT touch
 * the bus; the constraint is documented here for symmetry with `lvgl_port`
 * and `lcd_driver`.
 * ------------------------------------------------------------------------- */