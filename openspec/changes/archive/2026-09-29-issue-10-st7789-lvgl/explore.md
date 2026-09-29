# explore — issue #10 ST7789 + LVGL local screen

Issue #10 asks for ST7789 panel bring-up and a local LVGL UI. The surface
area is large and partially hardware-dependent. This is the ODD explore
phase: scope mapping, surface identification, and risk classification.

## Scope summary

- **Component target**: 4 new ESP-IDF components (`lcd_driver`,
  `lvgl_port`, `ui_screens`, `assets_brand`) wired into `main.c` via
  additive init calls. `display_manager` (from issue #9, PR #29) is
  reused.
- **Hardware dependencies**: ST7789V2 panel (4-wire SPI), ESP32-S3-WROOM-1-N16R8
  carrier with shared SPI bus to a future TF card slot, GPIO22 candidate for
  backlight (pending physical verification).
- **Firmware dependencies**: ESP-IDF v5.4, `esp_lcd` component (already in
  IDF), LVGL (added in a later Fase — contract-only here).
- **Forbidden surfaces**: `CLAUDE.md`, `project.md`, `Fases/`, `Tutorial/`,
  `knowledge/`, `docs/`, `docs_site/`, `firmware/gateway/CMakeLists.txt`,
  `sdkconfig.defaults`, `partitions.csv`, existing components,
  `wifi_manager/**`.

## Surface mapping (post-issue #9 baseline)

### Existing components (7)

| Component | Surface | Touched? |
|---|---|---|
| `board_profile` | flash/PSRAM probe via `nvs_config` | no |
| `board_rgb` | RGB LED control | no |
| `display_manager` | framebuffer constant + hardware hold | referenced (REQUIRES for lcd_driver) |
| `espnow_manager` | ESP-NOW backbone | no |
| `http_server` | REST API + WebSocket | no |
| `mqtt_bridge` | optional MQTT | no |
| `ota_manager` | OTA updates | no |

### New components (4)

| Component | Surface | Scope |
|---|---|---|
| `lcd_driver` | panel contact map + GPIO22 candidate | contract-only, no `esp_lcd_*` call |
| `lvgl_port` | LVGL adapter | contract-only, no `lv_init` |
| `ui_screens` | 6 screen stubs + orchestrator | contract-only, no widgets |
| `assets_brand` | placeholder label | ASCII constant + init |

### Files outside components (1)

- `firmware/gateway/main/main.c` — additive init chain (one file, +4 init calls).
- `firmware/gateway/main/CMakeLists.txt` — REQUIRES extended.
- `firmware/gateway/test/CMakeLists.txt` — defensive `list(FIND ...)` append for each new component.

## Risk classification

| Risk | Severity | Mitigation |
|---|---|---|
| Hardware verification (GPIO22→BLK) | medium | candidate macro, header note, no binding commit |
| SPI bus sharing with TF card | high | documented in header, no arbitration code in this slice |
| LVGL memory footprint | medium | deferred — contract-only, no `lv_init` |
| File/symbol name conflicts with existing components | low | `xtensa-esp32s3-elf-nm` audit per slice |
| Test runner config drift across slices | medium | defensive `list(FIND ...)` pattern verified post-#36 |
| Force-push blocked by Pi safety | low | feature-branch-chain avoids it |
| `gh pr create` body referencing "closed" PR | low | PR bodies describe the **issue**, not prior PR closures |

## Out of scope

- Real `esp_lcd_*` bring-up (deferred to a later Fase gated on physical
  carrier verification).
- Real LVGL wiring (`lv_init`, display driver, draw buffers, tick timer,
  `lv_task_handler`).
- Real screen implementations (each `ui_*_screen` stub logs the screen
  name and returns).
- Real brand assets (logo, palette, font).
- TF card driver (separate issue, owns the other side of the SPI bus).
- WiFi manager (issue #21, deferred).

## Acceptance criteria (mapped to ODD-1..ODD-6)

1. All 4 new components exist with their header + .c + CMakeLists + test stub.
2. `lcd_driver.h` carries the 8-pin contact map and GPIO22 candidate.
3. `lvgl_port.h` carries the SPI bus shared constraint.
4. `ui_screens.h` declares 6 screen functions + 1 orchestrator.
5. `assets_brand.h` defines `ASSETS_BRAND_PLACEHOLDER_LABEL "IIoT-Kit"`.
6. `main.c` calls all 4 init functions after `board_profile_read`.
7. `main/CMakeLists.txt` REQUIRES extended with the 4 new components.
8. `test/CMakeLists.txt` adds each new component to TEST_COMPONENTS.
9. No `esp_lcd_*` call, no `lv_init`, no draw buffer, no tick timer.
10. No GPIO assigned, no SPI bus init, no image decode.
11. Build success on `idf.py build` (main + test).
12. 11 Unity tags in `gateway_test.elf`.
13. 16 exported symbols in `gateway_test.elf`.
14. 0 component-level `Kconfig` files.
15. Boot smoke on `/dev/ttyACM0` shows 7 log lines in documented order.

## References

- GitHub issue: https://github.com/fgjcarlos/ESP32-IIoT-Kit/issues/10
- ODD feature document: `odd/tasks/issue-10-st7789-lvgl.md`
- Issue #9 archive: `openspec/changes/archive/2026-09-29-issue-9-gateway-firmware-foundation/`
- `CLAUDE.md` — hardware hold + GPIO22 candidate + display geometry facts.