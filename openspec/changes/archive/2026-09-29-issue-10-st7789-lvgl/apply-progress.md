# apply-progress — issue #10 ST7789 + LVGL local screen

This file tracks the apply phase progress slice-by-slice. Each slice is
one work-unit commit on its own feature branch, shipped via its own PR.
The chain is **feature-branch-chain**: no long-lived feature branch,
each slice lands on `main` independently.

## Slice 1 — ODD-1 `lcd_driver` (PR #31 MERGED)

- Branch: `feature/issue-10-lcd-driver`
- Commit: `4c80d2d feat(gateway): add lcd_driver component contract (interface only)`
- Merge: `f4b25db`
- Diff: +109/-0, 6 files.
- Evidence:
  - `gateway.bin`: 0x3a400 (92% free).
  - `gateway_test.bin`: 0x3c300 (76% free).
  - Unity tags: 8 (added `[lcd_driver]`).
  - Symbols: `lcd_driver_init` exported.
  - Kconfig: 0.
- Notes: First slice. Established the defensive `list(FIND ...)` append
  pattern in `firmware/gateway/test/CMakeLists.txt` for subsequent slices.

## Slice 2 — ODD-2 `lvgl_port` (PR #32 MERGED)

- Branch: `feature/issue-10-lvgl-port`
- Commit: `56ec735 feat(gateway): add lvgl_port component contract (interface only)`
- Merge: `0f9d21b`
- Diff: +134/-0, 6 files.
- Evidence:
  - `gateway.bin`: 0x3a400 (92% free, unchanged).
  - `gateway_test.bin`: 0x3c6c0 (76% free, +928 bytes).
  - Unity tags: 9 (added `[lvgl_port]`).
  - Symbols: `lvgl_port_init`, `lvgl_port_start`, `lvgl_port_stop` exported.
  - Kconfig: 0.
- Notes: Header documents the SPI bus shared constraint. No LVGL
  library is added to the build; `lvgl_port.c` does not call `lv_init`.

## Slice 3 — ODD-3 `ui_screens` (PR #33 MERGED)

- Branch: `feature/issue-10-ui-screens`
- Commit: `2a2ff28 feat(gateway): add ui_screens component contract (interface only)`
- Merge: `fe1608e`
- Diff: +151/-0, 6 files.
- Evidence:
  - `gateway.bin`: 0x3a400 (92% free, unchanged).
  - `gateway_test.bin`: 0x3ca60 (76% free, +928 bytes).
  - Unity tags: 10 (added `[ui_screens]`).
  - Symbols: 7 (`ui_screens_init` + 6 screen stubs).
  - Kconfig: 0.
- Notes: Test uses `TEST_ASSERT_NOT_NULL((void *)&ui_<name>_screen)` to
  force linker retention of empty stubs.

## Slice 4 — ODD-4 `assets_brand` (PR #34 MERGED)

- Branch: `feature/issue-10-assets-brand`
- Commit: `cb51bcc feat(gateway): add assets_brand component contract (interface only)`
- Merge: `7baab99`
- Diff: +97/-0, 5 component files.
- Evidence:
  - `gateway.bin`: 0x3a400 (92% free, unchanged).
  - `gateway_test.bin`: 0x3cdc0 (76% free, +864 bytes).
  - Unity tags: 11 in production linker map, but only 10 in test ELF
    (missing `[assets_brand]`, restored by ODD-6).
  - Symbols: `assets_brand_init` in production, but absent from test
    ELF until ODD-6.
  - Kconfig: 0.
- Notes: **This slice omitted the defensive `list(FIND ...)` append
  in `firmware/gateway/test/CMakeLists.txt`.** Production firmware
  unaffected. Bug caught in ODD-6 cumulative evidence, fixed by ODD-6.

## Slice 5 — ODD-5 `main.c` init chain (PR #35 MERGED)

- Branch: `feature/issue-10-main-init-chain`
- Commit: `0f6318a feat(gateway): wire display chain init chain into app_main (contracts only)`
- Merge: `351962f`
- Diff: +53/-1, 2 files (`firmware/gateway/main/main.c` + `firmware/gateway/main/CMakeLists.txt`).
- Evidence:
  - `gateway.bin`: 0x37970 (93% free, smaller because fresh build dir
    regenerated `sdkconfig` from `sdkconfig.defaults` via
    `idf.py set-target esp32s3`).
  - `gateway_test.bin`: 0x3ca80 (76% free).
  - Boot smoke: 7 log lines in order on `/dev/ttyACM0`.
  - Kconfig: 0.
- Notes: First physical hardware validation. ESP32-S3-WROOM-1-N16R8,
  chip rev v0.2, flash 16 MB, PSRAM 8 MB. Display chain produced
  log lines in the documented order (`lcd_driver` → `lvgl_port` →
  `ui_screens` → `assets_brand`).

## Slice 6 — ODD-6 cumulative evidence + ODD-4 fix (PR #36 MERGED)

- Branch: `feature/issue-10-cumulative-evidence`
- Commit: `1bcc1d9 test(gateway): register assets_brand in TEST_COMPONENTS (fix ODD-4 omission)`
- Merge: `8c8c806`
- Diff: +5/-0, 1 file (`firmware/gateway/test/CMakeLists.txt`).
- Evidence:
  - `gateway.bin`: 0x37970 (93% free, unchanged).
  - `gateway_test.bin`: 0x3cde0 (76% free, +864 bytes from ODD-5).
  - Unity tags: 11 (restored — `[assets_brand]` now present).
  - Symbols: 16 (restored — `assets_brand_init` now exported in test ELF).
  - Kconfig: 0.
  - Boot smoke: 7 log lines in order on `/dev/ttyACM0`, firmware
    version `351962f` (PR #35 merge commit).
- Notes: Final slice. Cumulative build/test + on-target boot smoke +
  ODD-4 test-registration fix. PR size intentionally small (5 lines)
  because the doc said "no code change" but a bug was caught and the
  fix was 5 lines.