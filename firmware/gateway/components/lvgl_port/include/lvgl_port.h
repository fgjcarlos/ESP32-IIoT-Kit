#pragma once

#include "esp_err.h"

/**
 * @brief Initialize the LVGL port contract.
 *
 * Interface only — full implementation is owned by the future display wiring
 * change, gated on physical hardware verification.
 *
 * @return ESP_OK for the contract-only placeholder.
 *
 * @note The contract does NOT call `lv_init`, does NOT register any display
 *       driver, does NOT allocate draw buffers, does NOT configure a tick
 *       timer, and does NOT call `lv_task_handler`. Real LVGL bring-up
 *       belongs in a later Fase and is gated on `lcd_driver` wiring plus
 *       physical carrier verification.
 *
 * @note The LVGL port is a thin adapter above `display_manager` (which owns
 *       the framebuffer constant `DISPLAY_MANAGER_FRAMEBUFFER_SIZE_BYTES`)
 *       and `lcd_driver` (which owns the panel contact map and the GPIO22
 *       backlight candidate). This header depends on both at the source
 *       level (`REQUIRES display_manager lcd_driver`) but it does NOT
 *       include their headers here — public callers should include them
 *       explicitly when they need the constants.
 *
 * @note The SPI bus shared constraint (LCD + future TF card) is documented
 *       as a runtime concern for a later Fase. No bus arbitration code
 *       lives in this contract.
 */
esp_err_t lvgl_port_init(void);

/**
 * @brief Start the LVGL port task / tick loop contract.
 *
 * Interface only — full implementation is owned by the future display wiring
 * change.
 *
 * @return ESP_OK for the contract-only placeholder.
 *
 * @note The contract does NOT call `lv_task_handler` and does NOT create a
 *       FreeRTOS task. Real tick loop belongs in a later Fase.
 */
esp_err_t lvgl_port_start(void);

/**
 * @brief Stop the LVGL port task / tick loop contract.
 *
 * Interface only — full implementation is owned by the future display wiring
 * change.
 *
 * @return ESP_OK for the contract-only placeholder.
 *
 * @note The contract does NOT delete a FreeRTOS task and does NOT call any
 *       LVGL teardown. Real shutdown belongs in a later Fase.
 */
esp_err_t lvgl_port_stop(void);

/* -------------------------------------------------------------------------
 * SPI bus shared constraint — LCD + future TF card.
 *
 * The display contract chain (`display_manager`, `lcd_driver`, `lvgl_port`,
 * `ui_screens`) shares the SPI bus with the TF card driver that will be
 * introduced in a later Fase. The TF card driver is **out of scope** for
 * issue #10; this contract does NOT include bus arbitration, mutex, or
 * ownership tracking. The constraint is documented here so the future TF
 * card driver has a clear signal that bus sharing must be coordinated at
 * design time, not invented ad-hoc.
 *
 * No code in this header or in `lvgl_port.c` touches the bus. The first
 * implementation that opens a SPI device (panel or card) is responsible for
 * the arbitration contract that the later TF card driver will rely on.
 * ------------------------------------------------------------------------- */