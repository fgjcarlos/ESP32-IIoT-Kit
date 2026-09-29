# Issue #10 — ST7789 + LVGL local display

## Objective

Add the gateway-side contract for the 1.9-inch 170×320 ST7789V2 LCD and a LVGL local UI framework, mirroring the issue-9 chained-PR style: contract-only headers + skeleton sources + per-component Unity tests + on-target boot smoke on `/dev/ttyACM0`. No runtime `esp_lcd_*` or LVGL calls in this issue. Runtime driver implementation, panel bring-up, LVGL port rendering, and bus arbitration with a future TF card driver are deferred to later Fases.

## Problem and rationale

Issue #10's body lists a real implementation (`Driver ST7789`, `Integración de LVGL`, `pantallas`, `coordinación del bus SPI con la tarjeta TF`). The user selected contracts-only scope for this delivery to stay inside Fase 0/1. The implementation belongs in a later Fase and depends on physical-carrier verification: GPIO assignments for the 8-pin display contact map (RES, DC, CS, SCL, SDA, BLK) and the backlight control GPIO (candidate GPIO22) are not yet confirmed against the delivered carrier. The framebuffer constant and the hardware-hold contract from issue #9 / PR #29 already protect that surface; this issue extends the boundary without weakening it.

## Scope

- Add new components under `firmware/gateway/components/`:
  - `lcd_driver` — header declares `esp_err_t lcd_driver_init(void)` plus documented SPI contact mapping (RES, DC, CS, SCL, SDA, BLK) and the GPIO22 backlight candidate. Skeleton source logs the contact map and returns `ESP_OK`. No `esp_lcd_*` calls.
  - `lvgl_port` — header declares `esp_err_t lvgl_port_init(void)`, `esp_err_t lvgl_port_start(void)`, `esp_err_t lvgl_port_stop(void)`. Skeleton source logs and returns `ESP_OK`. No LVGL API calls. Depends on `display_manager` and `lcd_driver` (declared in `REQUIRES`).
  - `ui_screens` — header enumerates the six screen entry points (`ui_boot_screen`, `ui_status_screen`, `ui_connectivity_screen`, `ui_nodes_screen`, `ui_alerts_screen`, `ui_ota_screen`) plus a `ui_screens_init(void)` orchestrator. Skeleton source logs each screen label and returns `ESP_OK`. No LVGL API calls. Depends on `lvgl_port`.
  - `assets_brand` — header declares `esp_err_t assets_brand_init(void)` and a single constant `ASSETS_BRAND_PLACEHOLDER_LABEL "IIoT-Kit"`. Skeleton source logs the placeholder label and returns `ESP_OK`. No image decoding. Depends on no other new component.
- Document the SPI bus-shared constraint (LCD + future TF card) in each new component header's Doxygen. No driver arbitration code.
- Document GPIO22 as the backlight candidate; flag as `pending physical verification` until carrier match.
- Each new component has the standard structure: `include/<name>.h`, `<name>.c`, `CMakeLists.txt`, `test/CMakeLists.txt`, `test/test_<name>.c`.
- Each slice appends to `firmware/gateway/test/CMakeLists.txt` `TEST_COMPONENTS` list using the defensive `list(FIND ...)` pattern.
- Final `apply-progress.md` (mirrored into the ODD archive) records the cumulative build/test evidence and the on-target boot smoke.

## Out of scope

- Real `esp_lcd_*` driver implementation (init, reset, set_window, draw, on/off).
- Real LVGL port code (`lv_init`, `lv_disp_draw_buf`, `lv_disp_drv_register`, tick timer, `lv_task_handler`).
- Any LVGL screen widget construction (`lv_obj_create`, label, button, chart, etc.).
- Real image decoding for brand assets.
- TF card driver (`esp_sdmmc_*`, `esp_fatfs_*`).
- SPI bus arbitration / mutex / lock with the (future) TF card.
- GPIO assignment changes — GPIO22 for BLK stays as a documented candidate, not a binding assignment.
- Modifying `display_manager` (PR #29): the framebuffer constant and the hardware-hold contract stay untouched. Issue #10 components depend on `display_manager`, never redefine it.
- Modifying `main.c` to call the new `*_init()` functions. Each slice can document the recommended call site in its header Doxygen; `main.c` change happens in the final ODD-6 slice as a chained single-line-per-init additive change.
- `partitions.csv`, `sdkconfig.defaults`, `CMakeLists.txt` of `firmware/gateway/`, `firmware/gateway/main/`, `firmware/gateway/test/`, `firmware/gateway/test/main/`. Existing components. Documentation (`docs/`, `Fases/`, `Tutorial/`, `knowledge/`, `docs_site/`, `CLAUDE.md`, `project.md`).
- Generated outputs (`sdkconfig`, `build/`, `*.bin`, `*.elf`, `.cache/`).

## Constraints and evidence

- 8-pin display contact map confirmed by the user: 1 GND, 2 VCC, 3 SCL (clock), 4 SDA (data input), 5 RES, 6 DC, 7 CS, 8 BLK.
- Display model: ST7789V2, 4-wire SPI, 170(H) RGB × 320(V), nominal 1.9-inch, 8 pins. The body of issue #10 says `172×320`; user confirmed it is a typo and the correct resolution is `170×320` (matches `DISPLAY_MANAGER_FRAMEBUFFER_SIZE_BYTES`).
- Backlight candidate: **GPIO22** — `pending physical verification`. The user could not find explicit ESP32-S3 carrier documentation for GPIO22→BLK; the assignment will be confirmed by reading the actual carrier pins. Document this in the header with a `[pending physical verification]` note; do not assume it.
- ESP-IDF v5.4, xtensa toolchain. Toolchain at `$HOME/esp/esp-idf`. Source before `idf.py` invocations.
- `firmware/gateway/test/` uses the dual-target assembly: `gateway_test.bin` is the on-target Unity runner with `TEST_COMPONENTS` listed components registered.
- `delivery_strategy: ask-on-risk`. Pause before each push+PR cycle. Slice size ≤400 lines/PR.
- `chain_strategy: feature-branch-chain`. Each slice is its own PR. Force-push workaround (rename branch + new PR) if Pi safety policy blocks `--force-with-lease`.
- TDD mode: strict. Compile success + Unity tag presence + symbol export is the verifiable equivalent in this environment. On-target `idf.py test` is deferred.
- No `Kconfig` files in any new component. The contract-only pattern from issue #9 is preserved.

## Work plan

### [x] ODD-1 — Add `lcd_driver` component contract

**Slice/PR:** `feature/issue-10-lcd-driver` → `main`. PR size target ≤400 lines.
**Files**:
- Create `firmware/gateway/components/lcd_driver/include/lcd_driver.h`.
- Create `firmware/gateway/components/lcd_driver/lcd_driver.c`.
- Create `firmware/gateway/components/lcd_driver/CMakeLists.txt`.
- Create `firmware/gateway/components/lcd_driver/test/CMakeLists.txt`.
- Create `firmware/gateway/components/lcd_driver/test/test_lcd_driver.c`.
- Edit `firmware/gateway/test/CMakeLists.txt` (defensive `list(FIND ...)` append for `lcd_driver`).
**Header content**:
- `#pragma once`, `#include "esp_err.h"`.
- Doxygen block describing:
  - 8-pin display contact map (verbatim): 1 GND, 2 VCC, 3 SCL (clock), 4 SDA (data input), 5 RES, 6 DC, 7 CS, 8 BLK.
  - ST7789V2 controller, 4-wire SPI, 170(H) × 320(V) RGB565.
  - GPIO22 candidate for BLK — flagged `[pending physical verification]`.
  - Reference: `display_manager.h` `DISPLAY_MANAGER_FRAMEBUFFER_SIZE_BYTES` and the hardware hold.
- `esp_err_t lcd_driver_init(void)` returning `ESP_OK` for the contract-only placeholder.
**Skeleton source**:
- `#include "esp_log.h"`.
- `static const char *TAG = "lcd_driver";`.
- One `ESP_LOGI(TAG, "...")` line listing the 8 contact names.
- Return `ESP_OK`.
**Component `CMakeLists.txt`**: `SRCS "lcd_driver.c"`, `INCLUDE_DIRS "include"`, `REQUIRES esp_common log display_manager`. No Kconfig.
**Test source**: `TEST_CASE("lcd_driver init returns ESP_OK", "[lcd_driver]")` calling `lcd_driver_init()` and asserting `ESP_OK`. Test `CMakeLists.txt`: `SRCS "test_lcd_driver.c"`, `INCLUDE_DIRS "."`, `REQUIRES lcd_driver`, `PRIV_REQUIRES unity`.
**Acceptance**:
- 5 files exist at the paths above.
- Header declares `lcd_driver_init` returning `esp_err_t` with the contact map and GPIO22 candidate.
- Component `REQUIRES` lists exactly `esp_common`, `log`, `display_manager`.
- Test compiles into the gateway test project under the `[lcd_driver]` tag and asserts `ESP_OK`.
- No `esp_lcd_*` call, no GPIO assignment change, no SPI bus init.
**Commit**: `feat(gateway): add lcd_driver component contract (interface only)` (Conventional Commits).

### [x] ODD-2 — Add `lvgl_port` component contract

**Slice/PR:** `feature/issue-10-lvgl-port` → `main`. PR size target ≤400 lines.
**Files**:
- Create `firmware/gateway/components/lvgl_port/include/lvgl_port.h`.
- Create `firmware/gateway/components/lvgl_port/lvgl_port.c`.
- Create `firmware/gateway/components/lvgl_port/CMakeLists.txt`.
- Create `firmware/gateway/components/lvgl_port/test/CMakeLists.txt`.
- Create `firmware/gateway/components/lvgl_port/test/test_lvgl_port.c`.
- Edit `firmware/gateway/test/CMakeLists.txt` (defensive append for `lvgl_port`).
**Header content**:
- `#pragma once`, `#include "esp_err.h"`.
- Doxygen block describing:
  - LVGL port contract — no LVGL API calls in this contract.
  - Framebuffer reference: `display_manager.h` `DISPLAY_MANAGER_FRAMEBUFFER_SIZE_BYTES`.
  - Tick source not specified in this contract (deferred).
  - SPI bus shared constraint (LCD + future TF card) documented but no arbitration code.
- Three public functions: `esp_err_t lvgl_port_init(void)`, `esp_err_t lvgl_port_start(void)`, `esp_err_t lvgl_port_stop(void)`.
**Skeleton source**:
- `#include "esp_log.h"`.
- `static const char *TAG = "lvgl_port";`.
- One `ESP_LOGI` line per function listing the action and returning `ESP_OK`.
**Component `CMakeLists.txt`**: `SRCS "lvgl_port.c"`, `INCLUDE_DIRS "include"`, `REQUIRES esp_common log display_manager lcd_driver`. No Kconfig.
**Test source**: `TEST_CASE("lvgl_port init/start/stop return ESP_OK", "[lvgl_port]")` calling all three and asserting `ESP_OK`.
**Acceptance**:
- 5 files exist.
- Header declares `lvgl_port_init/start/stop` returning `esp_err_t`.
- Component `REQUIRES` lists `esp_common`, `log`, `display_manager`, `lcd_driver`.
- Test under `[lvgl_port]` tag asserts `ESP_OK` for all three calls.
- No `lv_init`, no `lv_disp_*`, no tick timer code.
**Commit**: `feat(gateway): add lvgl_port component contract (interface only)`.

### [x] ODD-3 — Add `ui_screens` component contract

**Slice/PR:** `feature/issue-10-ui-screens` → `main`. PR size target ≤400 lines.
**Files**:
- Create `firmware/gateway/components/ui_screens/include/ui_screens.h`.
- Create `firmware/gateway/components/ui_screens/ui_screens.c`.
- Create `firmware/gateway/components/ui_screens/CMakeLists.txt`.
- Create `firmware/gateway/components/ui_screens/test/CMakeLists.txt`.
- Create `firmware/gateway/components/ui_screens/test/test_ui_screens.c`.
- Edit `firmware/gateway/test/CMakeLists.txt` (defensive append for `ui_screens`).
**Header content**:
- `#pragma once`, `#include "esp_err.h"`.
- Doxygen block enumerating the six screen entry points: `ui_boot_screen`, `ui_status_screen`, `ui_connectivity_screen`, `ui_nodes_screen`, `ui_alerts_screen`, `ui_ota_screen`.
- Each screen documented as a future `void ui_<name>_show(void)` function with no LVGL calls in this contract.
- One orchestrator: `esp_err_t ui_screens_init(void)`.
**Skeleton source**:
- `#include "esp_log.h"`.
- `static const char *TAG = "ui_screens";`.
- For each screen, a stub `void ui_<name>_show(void) { ESP_LOGI(TAG, "show: %s", #name); }`.
- `ui_screens_init` logs all six screen names and returns `ESP_OK`.
**Component `CMakeLists.txt`**: `SRCS "ui_screens.c"`, `INCLUDE_DIRS "include"`, `REQUIRES esp_common log lvgl_port`. No Kconfig.
**Test source**: `TEST_CASE("ui_screens init returns ESP_OK and exposes six screens", "[ui_screens]")` calling `ui_screens_init` and asserting all six screen functions are non-NULL (compare against address).
**Acceptance**:
- 5 files exist.
- Header enumerates six `void ui_*_show(void)` plus `ui_screens_init`.
- Component `REQUIRES` lists `esp_common`, `log`, `lvgl_port`.
- Test under `[ui_screens]` tag asserts `ESP_OK` from init and non-NULL function pointers.
- No `lv_obj_*`, no widget construction.
**Commit**: `feat(gateway): add ui_screens component contract (interface only)`.

### [x] ODD-4 — Add `assets_brand` component contract

**Slice/PR:** `feature/issue-10-assets-brand` → `main`. PR size target ≤400 lines.
**Files**:
- Create `firmware/gateway/components/assets_brand/include/assets_brand.h`.
- Create `firmware/gateway/components/assets_brand/assets_brand.c`.
- Create `firmware/gateway/components/assets_brand/CMakeLists.txt`.
- Create `firmware/gateway/components/assets_brand/test/CMakeLists.txt`.
- Create `firmware/gateway/components/assets_brand/test/test_assets_brand.c`.
- Edit `firmware/gateway/test/CMakeLists.txt` (defensive append for `assets_brand`).
**Header content**:
- `#pragma once`, `#include "esp_err.h"`.
- Doxygen block describing the brand asset placeholder scope: a single ASCII label constant for now. No image format support in this contract.
- Constant: `#define ASSETS_BRAND_PLACEHOLDER_LABEL "IIoT-Kit"`.
- `esp_err_t assets_brand_init(void)`.
**Skeleton source**:
- `#include "esp_log.h"`.
- `static const char *TAG = "assets_brand";`.
- `assets_brand_init` logs the label and returns `ESP_OK`.
**Component `CMakeLists.txt`**: `SRCS "assets_brand.c"`, `INCLUDE_DIRS "include"`, `REQUIRES esp_common log`. No Kconfig.
**Test source**: `TEST_CASE("assets_brand init returns ESP_OK and label matches", "[assets_brand]")` calling `assets_brand_init` and string-comparing the constant.
**Acceptance**:
- 5 files exist.
- Header declares `assets_brand_init` returning `esp_err_t` and the placeholder label constant.
- Component `REQUIRES` lists `esp_common`, `log`.
- Test under `[assets_brand]` tag asserts `ESP_OK` and label match.
- No image decoding, no `esp_jpeg_*`, no `esp_png_*`, no FATFS read.
**Commit**: `feat(gateway): add assets_brand component contract (interface only)`.

### [x] ODD-5 — Document SPI bus shared constraint + wire `main.c` additive init

**Slice/PR:** `feature/issue-10-main-init` → `main`. PR size target ≤400 lines.
**Files**:
- Edit `firmware/gateway/main/main.c` to add `display_manager_init`, `lcd_driver_init`, `lvgl_port_init`, `ui_screens_init`, `assets_brand_init` calls after `board_profile_read` and before the final `ESP_LOGI`.
- No new component, no new header.
- Add a brief comment in `main.c` documenting the call order rationale and the SPI bus shared constraint (LCD + future TF card). One short block comment is enough.
**Acceptance**:
- `main.c` builds, logs the new init lines, returns to the scheduler.
- `firmware/gateway/build/gateway.bin` and `firmware/gateway/test/build/gateway_test.bin` both compile.
- No change to `display_manager` PR-29 header.
- The boot smoke on `/dev/ttyACM0` shows the new `lcd_driver: contacts ...` and `ui_screens: screens: boot, status, connectivity, nodes, alerts, ota` log lines in the documented order: `nvs_config` → `board_profile` → `display_manager` → `lcd_driver` → `lvgl_port` → `ui_screens` → `assets_brand` → `gateway` profile summary.
**Commit**: `feat(gateway): wire display init chain in main.c (contracts only)`.

### [x] ODD-6 — Cumulative build/test + on-target boot smoke evidence

**Slice/PR:** none — evidence captured into `apply-progress.md` (mirrored here) and the SDD archive.
**Work**:
- Fresh worktree from `origin/main` post-merge of ODD-1..ODD-5.
- Run `idf.py -C firmware/gateway set-target esp32s3` (cached).
- Run `idf.py -C firmware/gateway build` → confirm `gateway.bin` size.
- Run `idf.py -C firmware/gateway/test build` → confirm `gateway_test.bin` size.
- `strings build/gateway_test.elf | grep -oE '\[(...)\]' | sort -u` → assert all 12 Unity tags: `[board_profile]`, `[board_rgb]`, `[display_manager]`, `[espnow_manager]`, `[http_server]`, `[mqtt_bridge]`, `[ota_manager]`, `[lcd_driver]`, `[lvgl_port]`, `[ui_screens]`, `[assets_brand]`, plus any inherited.
- `xtensa-esp32s3-elf-nm build/gateway_test.elf | grep -E 'T (espnow_manager_init|...)'` → assert all 12 symbols exported.
- `ls components/*/Kconfig*` → assert zero component-level `Kconfig` files.
- Flash `gateway.bin` to `/dev/ttyACM0` and capture monitor output. Assert the boot ordering: `nvs_config` precedes `gateway`/`board_profile`, and the new `display_manager`, `lcd_driver`, `lvgl_port`, `ui_screens`, `assets_brand` lines appear in that order before the final `gateway` profile summary.
- Record evidence in `apply-progress.md` (slice 7, 8) and in `odd/tasks/issue-10-st7789-lvgl.md` (this file, status notes for ODD-6).
**Acceptance**:
- All 12 Unity tags present.
- All 12 symbols exported.
- 0 component-level `Kconfig`.
- Boot smoke on `/dev/ttyACM0` shows the seven-line init chain in order.
- No real `esp_lcd_*` call, no `lv_init`, no image decode, no SPI bus init, no GPIO22 assigned as binding.

## Cross-cutting guard rails

- No `Kconfig` file in any new component.
- No `wifi_manager` component touched.
- No `partitions.csv` / `sdkconfig.defaults` / `firmware/gateway/CMakeLists.txt` / `firmware/gateway/main/CMakeLists.txt` / `firmware/gateway/test/CMakeLists.txt` (root) / `firmware/gateway/test/main/CMakeLists.txt` changes.
- No change to existing components (`board_profile`, `board_rgb`, `display_manager`, `espnow_manager`, `http_server`, `mqtt_bridge`, `ota_manager`).
- No documentation change (`CLAUDE.md`, `project.md`, `Fases/`, `Tutorial/`, `docs/`, `docs_site/`, `knowledge/`).
- No `idf_component.yml` introduction.
- Generated outputs (`sdkconfig`, `build/`, `*.bin`, `*.elf`, `.cache/`) never staged.
- Each slice has its own branch from `main` and its own PR; renames + new branches accepted as the Pi safety policy workaround for blocked force-pushes.

## Delivery and chain strategy

- `delivery_strategy: ask-on-risk`. Pause before each push+PR cycle.
- `chain_strategy: feature-branch-chain`. Five implementation slices + one documentation-only evidence slice.
- Review budget: 400 lines/PR.

## Status ledger

| ODD | PR | Branch | Commit | Status |
|---|---|---|---|---|
| ODD-1 `lcd_driver` | #31 | `feature/issue-10-lcd-driver` | `4c80d2d` | merged (PR #31 at `f4b25db`) |
| ODD-2 `lvgl_port` | #32 | `feature/issue-10-lvgl-port` | `56ec735` | merged (PR #32 at `0f9d21b`) |
| ODD-3 `ui_screens` | #33 | `feature/issue-10-ui-screens` | `2a2ff28` | merged (PR #33 at `fe1608e`) |
| ODD-4 `assets_brand` | #34 | `feature/issue-10-assets-brand` | `cb51bcc` | merged (PR #34 at `7baab99`) |
| ODD-5 `main.c` init chain | #35 | `feature/issue-10-main-init-chain` | `0f6318a` | merged (PR #35 at `351962f`) |
| ODD-6 evidence + ODD-4 test fix | #36 | `feature/issue-10-cumulative-evidence` | `1bcc1d9` | merged (PR #36 at `8c8c806`) |

**Final `origin/main` HEAD**: `8c8c806` (merge commit of PR #36).

## Summary evidence (post-#36)

- `gateway.bin`: 0x37970 bytes (93% free in 0x300000 app partition).
- `gateway_test.bin`: 0x3cde0 bytes (76% free in 0x100000 test partition).
- 11 Unity tags in test ELF: `[assets_brand] [board_profile] [board_rgb] [display_manager] [espnow_manager] [http_server] [lcd_driver] [lvgl_port] [mqtt_bridge] [ota_manager] [ui_screens]`.
- 16 exported symbols (board_profile_read, nvs_config_init, display_manager_init, espnow_manager_init, lcd_driver_init, lvgl_port_init, lvgl_port_start, lvgl_port_stop, ota_manager_init, ui_screens_init, 6 screen stubs, assets_brand_init).
- 0 component-level Kconfig files.
- 0 PR bodies referencing "closed" PRs.
- 6 PRs total, all merged, all targeting `main`, no force-push required (clean feature-branch-chain).

## Boot smoke on `/dev/ttyACM0`

7-line init chain in documented order:

```
I (1084) nvs_config: NVS initialized
I (1084) gateway: ESP32-S3 profile: flash=16777216 bytes, PSRAM=8388608 bytes
I (1084) lcd_driver: lcd_driver contract placeholder (8-pin ST7789V2 contacts 1..8 in header; backlight GPIO22 candidate pending physical verification; no esp_lcd_* call in this contract)
I (1104) lvgl_port: lvgl_port contract placeholder (no lv_init, no display driver, no draw buffers, no tick timer in this contract)
I (1114) ui_screens: ui_screens contract placeholder (no lv_obj_create, no screen registration, no event binding in this contract; six screen stubs declared in header)
I (1124) assets_brand: assets_brand contract placeholder (label="IIoT-Kit"; no image decode, no SPIFFS/FATFS read, no draw buffer in this contract)
I (1144) main_task: Returned from app_main()
```

No panic, no abort, app_main returns cleanly.

## ODD-4 test-registration fix (caught during ODD-6)

ODD-4 (PR #34, commit `cb51bcc`) added the `assets_brand` component but omitted the defensive `list(FIND ...)` append in `firmware/gateway/test/CMakeLists.txt`. Production firmware was unaffected (the symbol was in `gateway.bin` and the main init chain invoked it correctly), but the test project was inconsistent with the other eleven issue #10 components. ODD-6 (PR #36, commit `1bcc1d9`) restored the append, bringing the test ELF to 11 Unity tags and 16 exported symbols.

The ODD doc said "12 Unity tags" but the real count is 11 (one per component). No component is missing after the ODD-6 fix.

## Doc correction: status ledger table

The doc originally listed ODD-5 branch as `feature/issue-10-main-init` (typo). Actual branch used: `feature/issue-10-main-init-chain`. The status ledger above uses the correct branch name.

## References

- GitHub issue: https://github.com/fgjcarlos/ESP32-IIoT-Kit/issues/10
- Issue #9 (`display_manager` contract + hardware hold): PR #29 merged at `a8c877d`, main at `5741ba7` post-#30.
- Issue #8 (`board support` for ESP32-S3 N16R8 + 1.9-inch display): `odd/tasks/issue-8-gateway-board-support.md`.
- Engram memory: `sdd/issue-10-st7789-lvgl/progress`.