# proposal — issue #10 ST7789 + LVGL local screen

## Outcome

Ship a **contracts-only** foundation for the local LVGL UI on the
ESP32-S3 gateway. Mirrors the issue #9 pattern: 4 new ESP-IDF components
ship header contracts and skeleton source, plus `main.c` wires them in.
No real `esp_lcd_*` call, no `lv_init`, no draw buffer, no tick timer.
The full implementation is deferred to a later Fase gated on physical
carrier verification.

## Why contracts-only

Three reasons:

1. **Hardware hold**: the user-supplied 8-pin contact map and GPIO22
   backlight candidate are user-reported, not manufacturer-verified.
   Committing to a binding GPIO assignment (e.g., GPIO22→BLK) without
   physical verification would lock a possibly-wrong wiring into the
   codebase. The contract-only slice pins the candidate macro and the
   contact map in headers but does not exercise them.

2. **SPI bus shared constraint**: the display chain shares the SPI bus
   with a future TF card driver. The first component to bring up the
   bus will own the bus init and arbitration contract. Committing SPI
   bus init in this slice would risk a conflict with that future
   driver. The contract-only slice documents the constraint in
   `lvgl_port.h` and `lcd_driver.h` but does not touch the bus.

3. **LVGL bring-up is substantial**: `lv_init`, display driver
   registration, draw buffer allocation, tick timer configuration,
   `lv_task_handler`, FreeRTOS task creation, screen widget trees, and
   event binding are several days of work even with the headers in
   place. Folding them into the contract-only slice would inflate PR
   sizes past the 400-line review budget and mix contracts with
   implementations. The contract-only slice gives the review a clean
   surface: "this is the shape of the future code, but the future code
   itself is not here yet."

## Slice plan (chained-PR feature-branch-chain)

| ODD | Component | Files | Lines (target) | PR |
|---|---|---|---|---|
| ODD-1 | `lcd_driver` | 5 + 1 defensive | ~110 | #31 |
| ODD-2 | `lvgl_port` | 5 + 1 defensive | ~135 | #32 |
| ODD-3 | `ui_screens` | 5 + 1 defensive | ~150 | #33 |
| ODD-4 | `assets_brand` | 5 + 1 defensive | ~95 | #34 |
| ODD-5 | `main.c` + `main/CMakeLists.txt` | 2 | ~55 | #35 |
| ODD-6 | cumulative evidence + ODD-4 test fix | 1 | ~5 | #36 |

Total: 6 PRs, well within the 400-line review budget per PR.

## What gets shipped

### ODD-1 `lcd_driver`

- Header declares `lcd_driver_init()` returning `ESP_OK`.
- Header carries the 8-pin contact map verbatim (1 GND, 2 VCC, 3 SCL,
  4 SDA, 5 RES, 6 DC, 7 CS, 8 BLK).
- Header defines `LCD_DRIVER_BACKLIGHT_GPIO_CANDIDATE 22` flagged
  `[pending physical verification]`.
- Header references `display_manager.h` `DISPLAY_MANAGER_FRAMEBUFFER_SIZE_BYTES`
  (170×320×2 = 108,800 bytes) and the hardware hold.
- Skeleton source logs the contact names and returns.
- Component `REQUIRES esp_common log display_manager`.
- No Kconfig.

### ODD-2 `lvgl_port`

- Header declares `lvgl_port_init / start / stop`, all returning `ESP_OK`.
- Header Doxygen documents the SPI bus shared constraint and the no-go
  list (no `lv_init`, no display driver, no draw buffers, no tick
  timer, no `lv_task_handler`).
- Component `REQUIRES esp_common log display_manager lcd_driver`. No
  Kconfig.

### ODD-3 `ui_screens`

- Header declares `ui_screens_init` plus 6 screen stubs:
  `ui_boot_screen`, `ui_status_screen`, `ui_connectivity_screen`,
  `ui_nodes_screen`, `ui_alerts_screen`, `ui_ota_screen`.
- Each stub logs `show: <name>` and returns.
- Component `REQUIRES esp_common log lvgl_port`. No Kconfig.

### ODD-4 `assets_brand`

- Header declares `assets_brand_init` returning `ESP_OK`.
- Header defines `ASSETS_BRAND_PLACEHOLDER_LABEL "IIoT-Kit"`.
- Component `REQUIRES esp_common log`. No Kconfig. **Note: ODD-4
  omitted the defensive `list(FIND ...)` append in
  `firmware/gateway/test/CMakeLists.txt`; ODD-6 fixes this.**

### ODD-5 `main.c` init chain

- `main.c` calls `lcd_driver_init`, `lvgl_port_init`, `ui_screens_init`,
  `assets_brand_init` after `board_profile_read`.
- Each call is error-gated with `if (err != ESP_OK) { ESP_LOGE; return; }`.
- `main/CMakeLists.txt` REQUIRES extended with the 4 new components.

### ODD-6 cumulative evidence

- Cumulative build/test on `origin/main` post-#35.
- Boot smoke on `/dev/ttyACM0` (ESP32-S3-WROOM-1-N16R8).
- Fix ODD-4 test-registration omission (5 lines, +1 file).
- Document the 11 tags / 16 symbols / 0 Kconfig in `apply-progress.md`
  and `verify-report.md`.

## Non-goals

- Real `esp_lcd_*` bring-up.
- Real LVGL wiring.
- Real screen implementations.
- Real brand assets.
- TF card driver (separate issue, owns the other side of the SPI bus).
- WiFi manager (issue #21, deferred).

## References

- GitHub issue: https://github.com/fgjcarlos/ESP32-IIoT-Kit/issues/10
- ODD feature document: `odd/tasks/issue-10-st7789-lvgl.md`
- `CLAUDE.md` — gateway memory facts.