# tasks — issue #10 ST7789 + LVGL local screen

| ID | Description | PR | Status |
|---|---|---|---|
| ODD-1 | Add `lcd_driver` component contract (interface only) | #31 | [x] |
| ODD-2 | Add `lvgl_port` component contract (interface only) | #32 | [x] |
| ODD-3 | Add `ui_screens` component contract (interface only) | #33 | [x] |
| ODD-4 | Add `assets_brand` component contract (interface only) | #34 | [x] |
| ODD-5 | Wire display chain init chain into `app_main` (contracts only) | #35 | [x] |
| ODD-6 | Cumulative evidence + ODD-4 test-registration fix | #36 | [x] |

## Per-task summary

### ODD-1 `lcd_driver` — PR #31

- Branch: `feature/issue-10-lcd-driver`
- Commit: `4c80d2d feat(gateway): add lcd_driver component contract (interface only)`
- Merge commit: `f4b25db`
- Diff: +109/-0, 6 files.
- Files: `components/lcd_driver/{CMakeLists.txt, lcd_driver.c, include/lcd_driver.h, test/CMakeLists.txt, test/test_lcd_driver.c}` + `firmware/gateway/test/CMakeLists.txt`.
- Build: `gateway.bin` 0x3a400 (92% free), `gateway_test.bin` 0x3c300 (76% free).
- Tags: 8 (added `[lcd_driver]`).
- Symbols: +1 (`lcd_driver_init`).

### ODD-2 `lvgl_port` — PR #32

- Branch: `feature/issue-10-lvgl-port`
- Commit: `56ec735 feat(gateway): add lvgl_port component contract (interface only)`
- Merge commit: `0f9d21b`
- Diff: +134/-0, 6 files.
- Files: `components/lvgl_port/{CMakeLists.txt, lvgl_port.c, include/lvgl_port.h, test/CMakeLists.txt, test/test_lvgl_port.c}` + `firmware/gateway/test/CMakeLists.txt`.
- Build: `gateway.bin` 0x3a400 (92% free), `gateway_test.bin` 0x3c6c0 (76% free).
- Tags: 9 (added `[lvgl_port]`).
- Symbols: +3 (`lvgl_port_init`, `lvgl_port_start`, `lvgl_port_stop`).

### ODD-3 `ui_screens` — PR #33

- Branch: `feature/issue-10-ui-screens`
- Commit: `2a2ff28 feat(gateway): add ui_screens component contract (interface only)`
- Merge commit: `fe1608e`
- Diff: +151/-0, 6 files.
- Files: `components/ui_screens/{CMakeLists.txt, ui_screens.c, include/ui_screens.h, test/CMakeLists.txt, test/test_ui_screens.c}` + `firmware/gateway/test/CMakeLists.txt`.
- Build: `gateway.bin` 0x3a400 (92% free), `gateway_test.bin` 0x3ca60 (76% free).
- Tags: 10 (added `[ui_screens]`).
- Symbols: +7 (`ui_screens_init` + 6 screen stubs).

### ODD-4 `assets_brand` — PR #34

- Branch: `feature/issue-10-assets-brand`
- Commit: `cb51bcc feat(gateway): add assets_brand component contract (interface only)`
- Merge commit: `7baab99`
- Diff: +97/-0, 5 component files + 1 append.
- Files: `components/assets_brand/{CMakeLists.txt, assets_brand.c, include/assets_brand.h, test/CMakeLists.txt, test/test_assets_brand.c}`.
- **Note**: ODD-4 omitted the defensive `list(FIND ...)` append in
  `firmware/gateway/test/CMakeLists.txt`. Production firmware was
  unaffected, but the test project was inconsistent. ODD-6 fixes it.
- Build: `gateway.bin` 0x3a400 (92% free), `gateway_test.bin` 0x3cdc0 (76% free).
- Tags: 11 in production linker map, but only 10 in test ELF (missing
  `[assets_brand]`, restored by ODD-6).
- Symbols: +1 in production (`assets_brand_init`), but absent from
  test ELF until ODD-6.

### ODD-5 `main.c` init chain — PR #35

- Branch: `feature/issue-10-main-init-chain`
- Commit: `0f6318a feat(gateway): wire display chain init chain into app_main (contracts only)`
- Merge commit: `351962f`
- Diff: +53/-1, 2 files (`firmware/gateway/main/main.c` + `firmware/gateway/main/CMakeLists.txt`).
- Build: `gateway.bin` 0x37970 (93% free), `gateway_test.bin` 0x3ca80 (76% free).
- Boot smoke: 7 log lines in order on `/dev/ttyACM0`.

### ODD-6 cumulative evidence + ODD-4 fix — PR #36

- Branch: `feature/issue-10-cumulative-evidence`
- Commit: `1bcc1d9 test(gateway): register assets_brand in TEST_COMPONENTS (fix ODD-4 omission)`
- Merge commit: `8c8c806`
- Diff: +5/-0, 1 file (`firmware/gateway/test/CMakeLists.txt`).
- Build: `gateway.bin` 0x37970 (93% free), `gateway_test.bin` 0x3cde0 (76% free).
- Tags: 11 in test ELF (restored).
- Symbols: 16 in test ELF (restored).
- Kconfig: 0 component-level.
- Boot smoke: 7 log lines in order on `/dev/ttyACM0`, firmware version `351962f` (PR #35 merge commit).