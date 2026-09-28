# Tasks: issue-9-gateway-firmware-foundation

**Change**: issue-9-gateway-firmware-foundation
**Date**: 2026-09-27
**Artifact store**: openspec
**Delivery strategy**: ask-on-risk
**Chain strategy**: deferred
**Review budget**: 400 changed lines
**Educational phase**: Fase 0 → Fase 1 (gateway nucleus contract seam)
**strict_tdd**: false (per `openspec/config.yaml` — repository-level test command does not cover every in-scope project; phase keeps behavior-first checks proportionate to contract-only work; focused tests live alongside each implementation)

## Review Workload Forecast

| Field | Value |
|-------|-------|
| Estimated changed lines | ~630 (additions + deletions) across ~28 files |
| 400-line budget risk | High |
| Chained PRs recommended | Yes |
| Delivery strategy | ask-on-risk |
| Chain strategy | deferred |

```text
Decision needed before apply: Yes
Chained PRs recommended: Yes
Chain strategy: deferred
400-line budget risk: High
```

**Forecast rationale.** Aggregate ≈28 files (5 new component directories × 5 files each, plus 2 new `nvs_config` files, plus 3 small edits to existing files) × ~15–25 lines/file ≈ ~630 lines. Aggregate exceeds the 400-line budget; any single component contract task (T-01.01 … T-01.05) stays ≈100–120 lines, comfortably inside budget. T-00.01 (NVS) ≈70 lines. T-01.06 (TEST_COMPONENTS append) ≈10 lines. T-01.07 and T-01.08 are evidence-only. Apply phase MUST pause on `ask-on-risk` before opening PRs and MUST NOT invent a chain strategy or grant `size:exception`.

**Slicing coherence (important — read before slicing).** `T-00.01` (NVS) is independent and may stand alone as its own PR (≈70 lines). The five component contract tasks (T-01.01 … T-01.05) and `T-01.06` (TEST_COMPONENTS registration) **cannot** be split as "all components first, registration last": the gateway test project can only collect a component's `TEST_CASE` after the component is in `TEST_COMPONENTS`, and the test project only compiles successfully when every registered component's `test/test_<name>.c` exists on disk. Therefore:

- A standalone `T-01.06` (registration with no component code in the same diff) is INVALID — it would register components whose test sources do not exist yet, breaking the test build.
- A "Wave A + Wave G" early slice (T-00.01 + T-01.06 alone) is INVALID for the same reason. Do not propose or open that PR.
- Two valid integration patterns satisfy the dependency:

  1. **Chained one-component-per-PR.** Each PR contains exactly one of `T-01.01 … T-01.05` and incrementally appends that component to `TEST_COMPONENTS` in the same diff. The "standalone" `T-01.06` task is folded into each component PR; the cumulative registration is what the per-PR `TEST_COMPONENTS` append produces. Final review of `test/CMakeLists.txt` shows all five names appended in dependency order.
  2. **Single implementation slice.** `T-01.01 … T-01.05` plus `T-01.06` land together. Only valid with `size:exception` because the aggregate ≈570 lines exceeds the 400-line budget.

Apply phase selects one pattern under `ask-on-risk`. Until then, `T-01.06`'s diff is the set of five `list(FIND TEST_COMPONENTS "<name>" ...)` blocks following the existing `board_rgb` defensive pattern.

**Shared-file coordination with issue #21** (no file-level dependency; coordination only):

| Shared file | This change edits | Issue #21 edits | Sequencing rule |
|---|---|---|---|
| `firmware/gateway/main/main.c` | T-00.01 inserts `nvs_config_init()` as the first `app_main()` call, before `board_profile_read()`. | WUC3 appends `wifi_manager_init()` after `board_profile_read()`. | T-00.01 lands first; issue #21 appends after. Final order: `nvs_config_init()` → `board_profile_read()` → `wifi_manager_init()`. |
| `firmware/gateway/main/CMakeLists.txt` | T-00.01 adds `nvs_config.c` to `SRCS` and `nvs_flash` to `REQUIRES`. | WUC3 adds `wifi_manager` to `REQUIRES`. | Distinct lines; mergeable as separate edits. |
| `firmware/gateway/test/CMakeLists.txt` | T-01.06 (or its distributed form per chained-PR pattern) appends the five new components to `TEST_COMPONENTS`. | WUC1 appends `wifi_manager`. | Distinct appends; mergeable as separate edits. |

No other file overlaps between this change and issue #21.

---

## Guard rails (apply to every task below)

These constraints are normative. Any task that violates them is rejected at apply time.

- **No-touch paths** — do not create, edit, test, delete, or depend on any of:
  - `**/wifi_manager/**` (owned by issue #21; presence on disk is not a blocker; this change's invariant is only that no task touches any file under that directory).
  - `firmware/common/**`, `firmware/node/**`, `firmware/gateway/web/**`, `firmware/gateway/components/actuator_ctrl/**`.
  - `firmware/gateway/CMakeLists.txt`, `firmware/gateway/sdkconfig.defaults` (tracked source — out of scope, NOT a generated output), `firmware/gateway/partitions.csv`.
  - `firmware/gateway/components/board_profile/**`, `firmware/gateway/components/board_rgb/**`, `firmware/gateway/test/main/**`, existing `firmware/gateway/components/board_*/test/**`.
  - All documentation: `CLAUDE.md`, `project.md`, `Fases/*`, `docs/replanificacion/*`, `docs/hardware/*`, `docs_site/*`, `Tutorial/*`, `knowledge/*`.
  - **Generated outputs that MUST remain untracked/unstaged**: `sdkconfig`, `sdkconfig.old`, `build/`, `*.bin`, `*.elf`, `.cache/`. (`sdkconfig.defaults` is tracked source configuration and is NOT a generated output — it is out of scope but for a different reason.)
  - Dirty/untracked paths on the current branch `chore/sdd-archive-iiot-kit-transformation`. Do not clean, reset, stage, commit, or touch them.
  - `openspec/changes/iiot-kit-transformation/**` and `openspec/changes/archive/2026-09-27-iiot-kit-transformation/**` (reference only).
- **No git-state mutation in this artifact.** This tasks.md does not commit, push, open a PR, run `git clean` / `git reset --hard` / `git stash`, or create worktrees. Commit, push, PR creation, and worktree selection remain a later parent/user delivery decision. Planning may recommend worktree isolation; it cannot authorize or mandate git/worktree mutation.
- **No `Kconfig` files** introduced by this change for any of `espnow_manager`, `http_server`, `mqtt_bridge`, `ota_manager`, `display_manager`, or `nvs_config`.
- **No MQTT client link** — `mqtt_bridge` MUST NOT include any MQTT header, list `mqtt` in `REQUIRES`, or call `esp_mqtt_client_*`.
- **No display hardware assumptions** — `display_manager` MUST NOT assign GPIOs, write ST7789 registers, instantiate a backlight driver, or use the withdrawn `0.1155 × 0.1155 mm` pixel pitch or geometry derived from it. Hardware hold language is reproduced verbatim from the spec.
- **Out-of-scope runtime behavior** — no full AP+STA bring-up, ESP-NOW peer registration, REST endpoints, WebSocket frames, SPIFFS asset serving, MQTT publish/subscribe, OTA partition rotation, ST7789 driver, NVS typed accessors, sensor drivers, or SNTP. Each entry point is a contract-only placeholder.
- **Headers** follow `board_profile` / `board_rgb` conventions: `#pragma once`, `esp_err.h` include, `esp_err_t`-returning public functions, snake_case naming, source-local `TAG`. No ESP-IDF version guard.
- **Strict task dependencies** — each task lists its dependencies and is not eligible to start until they are complete. Apply phase runs tasks in dependency order.
- **Allowed edit surface per task** — each task lists exactly which files it may create or edit. Any other path is out of scope for that task.

---

## Fase 0 — Foundation

### T-00.01 — NVS bootstrap composition

- [x] T-00.01 — NVS bootstrap composition.

**Spec requirements**: "NVS Bootstrap Composition" — `nvs_config_init()` declaration, namespace table documentation, `mqtt_namespace` default key documentation; recoverable-erase retry pattern; first-operation semantics in `app_main()` with non-OK short-circuit; no typed accessors.

**Files affected**:
- Create: `firmware/gateway/main/nvs_config.h`.
- Create: `firmware/gateway/main/nvs_config.c`.
- Edit: `firmware/gateway/main/main.c` (insert `#include "nvs_config.h"` and call `nvs_config_init()` as the first operation in `app_main()`, before `board_profile_read()`; on non-OK return, log via `ESP_LOGE` with the existing `gateway` tag and `return;` immediately. Do NOT modify the existing `board_profile_read()` block or its surrounding code.).
- Edit: `firmware/gateway/main/CMakeLists.txt` (append `nvs_config.c` to `SRCS`; append `nvs_flash` to `REQUIRES`. Preserve the existing `board_profile` REQUIRES entry and the `INCLUDE_DIRS "."` line.).

**Dependencies**: None.

**Check command**: `cd firmware/gateway && idf.py build`. When the toolchain is unavailable, T-01.07 records the unavailable-toolchain condition explicitly.

**Description**: Add the `nvs_config` foundation in `main/` per Fase 1 T1.1.1 placement. `nvs_config.h` declares `esp_err_t nvs_config_init(void)` and documents the namespace table (`"wifi"`, `"mqtt"`, `"ota"`, `"display"`, `"node"`) and the documented default NVS key `mqtt_namespace = "iiot-kit"` sourced from `docs/replanificacion/02-protocolo-unificado.md`. `nvs_config.c` implements `nvs_config_init()` per the design algorithm: call `nvs_flash_init()`; on `ESP_ERR_NVS_NO_FREE_PAGES` or `ESP_ERR_NVS_NEW_VERSION_FOUND`, call `nvs_flash_erase()` once and retry `nvs_flash_init()` once; on success emit info-level log via the source-local `nvs_config` tag and return `ESP_OK`; on non-recoverable failure log via `ESP_LOGE` and return the final error. No namespace opens, key reads/writes, allocations, or buffered accessors. `main.c` calls `nvs_config_init()` first and returns on non-OK; the existing `board_profile_read()` block is preserved unchanged. `main/CMakeLists.txt` registers the new source and adds `nvs_flash` to `REQUIRES`.

**Acceptance criteria**:
- `firmware/gateway/main/nvs_config.{c,h}` exist with the documented namespace table, `mqtt_namespace` default key citation, and recoverable-erase retry pattern (no unbounded loop; exactly one erase-then-retry on the two recoverable codes).
- `main.c` calls `nvs_config_init()` as the first operation in `app_main()`; on non-OK, `app_main()` returns without calling `board_profile_read()`.
- `main/CMakeLists.txt` `SRCS` contains both `main.c` and `nvs_config.c`; `REQUIRES` contains both `board_profile` and `nvs_flash`. No other line of `main/CMakeLists.txt` is changed.
- Gateway app builds (`cd firmware/gateway && idf.py build`); unavailable-toolchain condition is recorded by T-01.07.
- No file outside the listed edit surface is modified.

---

## Fase 1 — Gateway nucleus contract seam

### T-01.01 — `espnow_manager` component contract

- [x] T-01.01 — `espnow_manager` component contract.

**Spec requirements**: "`espnow_manager` Component Contract" — header conventions, peer-table maximum-size constant `20`, skeleton compiles and logs contract-only state, contract test proves symbol existence.

**Files affected**:
- Create: `firmware/gateway/components/espnow_manager/include/espnow_manager.h`.
- Create: `firmware/gateway/components/espnow_manager/espnow_manager.c`.
- Create: `firmware/gateway/components/espnow_manager/CMakeLists.txt`.
- Create: `firmware/gateway/components/espnow_manager/test/CMakeLists.txt`.
- Create: `firmware/gateway/components/espnow_manager/test/test_espnow_manager.c`.

**Dependencies**: T-00.01 must have landed (or be in the same implementation slice) so the gateway app build can succeed when this task's component is added. See Slicing coherence: a chained-PR delivery integrating T-01.01 must also include the cumulative `TEST_COMPONENTS` append described in T-01.06.

**Check command**: `cd firmware/gateway/test && idf.py test`.

**Description**: Create the `espnow_manager` component mirroring the `board_profile` layout. Header declares `esp_err_t espnow_manager_init(void)` and `#define ESPNOW_MANAGER_MAX_PEERS 20U` with a brief Doxygen block naming Fase 1 T1.4.x as the owning runtime implementer. Skeleton source uses a source-local `static const char *TAG = "espnow_manager";`, emits a single `ESP_LOGI` placeholder identifying the contract-only state, and returns `ESP_OK`. No `esp_now_*` call, no `esp_now.h` include, no task/queue/buffer. Component `CMakeLists.txt` registers `SRCS "espnow_manager.c"`, `INCLUDE_DIRS "include"`, `REQUIRES esp_common log`. Test source contains one `TEST_CASE` tagged `[espnow_manager]` that calls `espnow_manager_init()` and asserts `ESP_OK`. Test `CMakeLists.txt` registers `SRCS "test_espnow_manager.c"`, `INCLUDE_DIRS "."`, `REQUIRES espnow_manager`, `PRIV_REQUIRES unity`.

**Acceptance criteria**:
- Five files exist at the paths above.
- Header contains `espnow_manager_init` returning `esp_err_t`, `ESPNOW_MANAGER_MAX_PEERS` defined as `20U`, and the Fase 1 T1.4.x ownership Doxygen block.
- Component `REQUIRES` lists exactly `esp_common` and `log`.
- Test compiles into the gateway test project, is collected under the `[espnow_manager]` tag, and asserts `ESP_OK`.
- No ESP-NOW runtime call is exercised.

### T-01.02 — `http_server` component contract

- [ ] T-01.02 — `http_server` component contract.

**Spec requirements**: "`http_server` Component Contract" — header conventions, public start/stop entry points, skeleton compiles and logs contract-only state, contract test proves symbol existence.

**Files affected**:
- Create: `firmware/gateway/components/http_server/include/http_server.h`.
- Create: `firmware/gateway/components/http_server/http_server.c`.
- Create: `firmware/gateway/components/http_server/CMakeLists.txt`.
- Create: `firmware/gateway/components/http_server/test/CMakeLists.txt`.
- Create: `firmware/gateway/components/http_server/test/test_http_server.c`.

**Dependencies**: T-00.01 (same rationale as T-01.01). Chained-PR integration requires the cumulative `TEST_COMPONENTS` append per T-01.06.

**Check command**: `cd firmware/gateway/test && idf.py test`.

**Description**: Create the `http_server` component mirroring the `board_profile` layout. Header declares `esp_err_t http_server_start(void)` and `esp_err_t http_server_stop(void)` with a brief Doxygen block naming Fase 4 as the owning runtime implementer. Skeleton source uses a source-local `static const char *TAG = "http_server";`; both entry points emit a single `ESP_LOGI` placeholder and return `ESP_OK`. No `httpd_*` call, no `esp_http_server.h` include, no SPIFFS mount. Component `CMakeLists.txt` registers `SRCS "http_server.c"`, `INCLUDE_DIRS "include"`, `REQUIRES esp_common log`. Test source contains one `TEST_CASE` tagged `[http_server]` that calls both entry points and asserts `ESP_OK` for each. Test `CMakeLists.txt` registers `SRCS "test_http_server.c"`, `INCLUDE_DIRS "."`, `REQUIRES http_server`, `PRIV_REQUIRES unity`.

**Acceptance criteria**:
- Five files exist at the paths above.
- Header contains both `http_server_start` and `http_server_stop` returning `esp_err_t`, and the Fase 4 ownership Doxygen block.
- Component `REQUIRES` lists exactly `esp_common` and `log`.
- Test compiles into the gateway test project, is collected under the `[http_server]` tag, and asserts `ESP_OK` for both entry points.
- No TCP socket, URI handler, WebSocket, or SPIFFS mount is exercised.

### T-01.03 — `mqtt_bridge` component contract

- [ ] T-01.03 — `mqtt_bridge` component contract.

**Spec requirements**: "`mqtt_bridge` Component Contract" — header conventions, public init entry point, skeleton compiles WITHOUT linking the ESP-IDF MQTT client, no Kconfig file, contract test proves symbol existence without runtime behavior.

**Files affected**:
- Create: `firmware/gateway/components/mqtt_bridge/include/mqtt_bridge.h`.
- Create: `firmware/gateway/components/mqtt_bridge/mqtt_bridge.c`.
- Create: `firmware/gateway/components/mqtt_bridge/CMakeLists.txt`.
- Create: `firmware/gateway/components/mqtt_bridge/test/CMakeLists.txt`.
- Create: `firmware/gateway/components/mqtt_bridge/test/test_mqtt_bridge.c`.

**Dependencies**: T-00.01 (same rationale). Chained-PR integration requires the cumulative `TEST_COMPONENTS` append per T-01.06.

**Check command**: `cd firmware/gateway/test && idf.py test`.

**Description**: Create the `mqtt_bridge` component mirroring the `board_profile` layout. Header declares `esp_err_t mqtt_bridge_init(void)` with a brief Doxygen block naming Fase 1 T1.5.x as the owning runtime implementer and noting Kconfig default-off gating ownership. Skeleton source uses a source-local `static const char *TAG = "mqtt_bridge";`, emits a single `ESP_LOGI` placeholder, and returns `ESP_OK`. NO `mqtt_client.h` include, NO `esp_mqtt_client_*` call. Component `CMakeLists.txt` registers `SRCS "mqtt_bridge.c"`, `INCLUDE_DIRS "include"`, `REQUIRES esp_common log`. The `mqtt` component is NOT in `REQUIRES`. No `Kconfig` or `Kconfig.projbuild` file is introduced anywhere under this component. Test source contains one `TEST_CASE` tagged `[mqtt_bridge]` that calls `mqtt_bridge_init()` and asserts `ESP_OK`. Test `CMakeLists.txt` registers `SRCS "test_mqtt_bridge.c"`, `INCLUDE_DIRS "."`, `REQUIRES mqtt_bridge`, `PRIV_REQUIRES unity`.

**Acceptance criteria**:
- Five files exist at the paths above.
- Header contains `mqtt_bridge_init` returning `esp_err_t` and the Fase 1 T1.5.x ownership Doxygen block.
- Component `REQUIRES` lists exactly `esp_common` and `log`; `mqtt` is NOT listed.
- No `Kconfig` or `Kconfig.projbuild` file exists under `firmware/gateway/components/mqtt_bridge/`.
- Test compiles into the gateway test project, is collected under the `[mqtt_bridge]` tag, and asserts `ESP_OK`.
- No broker socket, publish, or subscribe is exercised.

### T-01.04 — `ota_manager` component contract

- [ ] T-01.04 — `ota_manager` component contract.

**Spec requirements**: "`ota_manager` Component Contract" — header conventions, public init entry point, skeleton compiles and logs contract-only state, contract test proves symbol existence without runtime OTA behavior.

**Files affected**:
- Create: `firmware/gateway/components/ota_manager/include/ota_manager.h`.
- Create: `firmware/gateway/components/ota_manager/ota_manager.c`.
- Create: `firmware/gateway/components/ota_manager/CMakeLists.txt`.
- Create: `firmware/gateway/components/ota_manager/test/CMakeLists.txt`.
- Create: `firmware/gateway/components/ota_manager/test/test_ota_manager.c`.

**Dependencies**: T-00.01 (same rationale). Chained-PR integration requires the cumulative `TEST_COMPONENTS` append per T-01.06.

**Check command**: `cd firmware/gateway/test && idf.py test`.

**Description**: Create the `ota_manager` component mirroring the `board_profile` layout. Header declares `esp_err_t ota_manager_init(void)` with a brief Doxygen block naming Fase 5 T5.x as the owning runtime implementer. Skeleton source uses a source-local `static const char *TAG = "ota_manager";`, emits a single `ESP_LOGI` placeholder, and returns `ESP_OK`. NO `esp_ota_*` call, NO `esp_ota.h` include. Component `CMakeLists.txt` registers `SRCS "ota_manager.c"`, `INCLUDE_DIRS "include"`, `REQUIRES esp_common log`. No Kconfig file. Test source contains one `TEST_CASE` tagged `[ota_manager]` that calls `ota_manager_init()` and asserts `ESP_OK`. Test `CMakeLists.txt` registers `SRCS "test_ota_manager.c"`, `INCLUDE_DIRS "."`, `REQUIRES ota_manager`, `PRIV_REQUIRES unity`.

**Acceptance criteria**:
- Five files exist at the paths above.
- Header contains `ota_manager_init` returning `esp_err_t` and the Fase 5 T5.x ownership Doxygen block.
- Component `REQUIRES` lists exactly `esp_common` and `log`.
- Test compiles into the gateway test project, is collected under the `[ota_manager]` tag, and asserts `ESP_OK`.
- No partition write, bootable flag, or reboot is triggered.

### T-01.05 — `display_manager` component contract (hardware hold)

- [ ] T-01.05 — `display_manager` component contract (hardware hold).

**Spec requirements**: "`display_manager` Component Contract (interface only)" — header exposes documented contract and hardware hold verbatim, framebuffer size constant `170 × 320 × 2 = 108,800` bytes, skeleton compiles and logs contract-only state, contract test proves constant and symbol without runtime display behavior.

**Files affected**:
- Create: `firmware/gateway/components/display_manager/include/display_manager.h`.
- Create: `firmware/gateway/components/display_manager/display_manager.c`.
- Create: `firmware/gateway/components/display_manager/CMakeLists.txt`.
- Create: `firmware/gateway/components/display_manager/test/CMakeLists.txt`.
- Create: `firmware/gateway/components/display_manager/test/test_display_manager.c`.

**Dependencies**: T-00.01 (same rationale). Chained-PR integration requires the cumulative `TEST_COMPONENTS` append per T-01.06.

**Check command**: `cd firmware/gateway/test && idf.py test`.

**Description**: Create the `display_manager` component mirroring the `board_profile` layout. Header declares `esp_err_t display_manager_init(void)` and `#define DISPLAY_MANAGER_FRAMEBUFFER_SIZE_BYTES (170U * 320U * 2U)`. The header reproduces the spec's hardware-hold language verbatim (GPIO35/36/37 unavailable for external use due to N16R8 Octal PSRAM; GPIO48 reserved for onboard WS2812 RGB LED; GPIO19/20 reserved for native USB while needed; withdrawn `0.1155 × 0.1155 mm` pixel pitch forbidden, no geometry derived from it; display GPIO mapping / backlight current / control / reset polarity / logic levels unresolved). Doxygen block names the future display wiring change as the owning runtime implementer, gated on physical hardware verification. Skeleton source uses a source-local `static const char *TAG = "display_manager";`, emits a single `ESP_LOGI` placeholder, and returns `ESP_OK`. NO GPIO assignment, NO SPI peripheral, NO ST7789 command, NO backlight driver, NO framebuffer allocation. Component `CMakeLists.txt` registers `SRCS "display_manager.c"`, `INCLUDE_DIRS "include"`, `REQUIRES esp_common log`. No Kconfig file. Test source contains one `TEST_CASE` tagged `[display_manager]` that asserts `DISPLAY_MANAGER_FRAMEBUFFER_SIZE_BYTES == 170U * 320U * 2U`, calls `display_manager_init()`, and asserts `ESP_OK`. Test `CMakeLists.txt` registers `SRCS "test_display_manager.c"`, `INCLUDE_DIRS "."`, `REQUIRES display_manager`, `PRIV_REQUIRES unity`.

**Acceptance criteria**:
- Five files exist at the paths above.
- Header contains `display_manager_init` returning `esp_err_t`, `DISPLAY_MANAGER_FRAMEBUFFER_SIZE_BYTES` defined as `170U * 320U * 2U`, all six hardware-hold bullets verbatim, and the future display wiring change ownership Doxygen block.
- Component `REQUIRES` lists exactly `esp_common` and `log`; no SPI, GPIO, LCD, or display driver dependency.
- No `Kconfig` or `Kconfig.projbuild` file exists under `firmware/gateway/components/display_manager/`.
- Test compiles into the gateway test project, is collected under the `[display_manager]` tag, asserts the framebuffer constant, and asserts `ESP_OK` for the entry point.
- No GPIO, SPI, ST7789, or backlight API is exercised; no framebuffer memory is allocated.

### T-01.06 — TEST_COMPONENTS registration (cumulative, must accompany component PRs)

- [ ] T-01.06 — TEST_COMPONENTS registration (cumulative, must accompany component PRs).

**Spec requirements**: "Unity Test Skeleton Registration" — `TEST_COMPONENTS` lists the five new components alongside `board_profile;board_rgb`; defensive `list(FIND ...)` pattern preserved; no other entry from this change is appended; each registered component contributes at least one collectable `TEST_CASE`.

**Files affected**:
- Edit: `firmware/gateway/test/CMakeLists.txt` — extend `TEST_COMPONENTS` to include every component contract task (T-01.01 … T-01.05) whose source files are part of the same delivery slice. Preserve the existing `board_profile;board_rgb` cached default and the existing `board_rgb` `list(FIND ...)` defensive check. Use one additional `list(FIND TEST_COMPONENTS "<name>" ...)` / conditional `list(APPEND ...)` per name to preserve the file's existing duplicate-avoidance semantics. Do NOT change `EXTRA_COMPONENT_DIRS`, the `include($ENV{IDF_PATH}/tools/cmake/project.cmake)` line, or the `project(gateway_test)` line.

**Dependencies**: T-01.01, T-01.02, T-01.03, T-01.04, T-01.05 (every component's `test/test_<name>.c` must exist on disk before the test runner can collect its `TEST_CASE`).

**Check command**: `cd firmware/gateway/test && idf.py test`.

**Description**: Update `firmware/gateway/test/CMakeLists.txt` so that, at the moment any component contract code is added to the tree, the corresponding component is also conditionally appended to `TEST_COMPONENTS`. The standalone `T-01.06` task — `TEST_COMPONENTS` registration with no other component code in the same diff — is INVALID because the test runner would fail to find the registered component's `test/test_<name>.c` files. Two valid integration patterns (apply phase selects one under `ask-on-risk`):

- **Chained one-component-per-PR.** Each PR contains exactly one of `T-01.01 … T-01.05` and incrementally appends that component to `TEST_COMPONENTS` in the same diff. This task's standalone form is folded into each component PR; the cumulative registration is what the per-PR `TEST_COMPONENTS` append produces.
- **Single implementation slice.** `T-01.01 … T-01.05` plus `T-01.06` land together. Only valid with `size:exception` because the aggregate ≈570 lines exceeds the 400-line budget.

Until a delivery strategy is selected, this task's diff is the set of five `list(FIND TEST_COMPONENTS "<name>" ...)` blocks following the existing `board_rgb` defensive pattern, in the order `espnow_manager`, `http_server`, `mqtt_bridge`, `ota_manager`, `display_manager`.

**Acceptance criteria**:
- `TEST_COMPONENTS` includes every component whose `test/test_<name>.c` exists at apply time, alongside `board_profile;board_rgb`.
- The existing `board_rgb` defensive `list(FIND ...)` block is preserved unchanged.
- No other line of `firmware/gateway/test/CMakeLists.txt` is changed.
- The gateway test project builds and the test runner collects and reports at least one `TEST_CASE` per registered component.

### T-01.07 — Build / test evidence (compile-only)

- [ ] T-01.07 — Build / test evidence (compile-only).

**Spec requirements**: "Project-Local Build/Test Evidence (compile-only)" — three host-availability scenarios (toolchain + target reachable, toolchain missing, target unreachable). Each scenario's expected record format is reproduced verbatim from the spec.

**Files affected**: None (evidence-only; no source file edits).

**Dependencies**: T-00.01, T-01.01, T-01.02, T-01.03, T-01.04, T-01.05, T-01.06 (every component CMakeLists.txt must be on disk and every component must be in `TEST_COMPONENTS` for `idf.py test` to register them).

**Check commands**:
- `cd firmware/gateway && idf.py build`
- `cd firmware/gateway/test && idf.py test`

**Description**: Execute the two configured project-local commands exactly as written and record the full output verbatim in the verify-phase artifact. When the toolchain is unavailable, record the unavailable-toolchain condition explicitly (command not executed, reason: toolchain not present). When the toolchain is installed but the target is unreachable, separate compile/link output from flash/monitor output based on what the captured output actually shows — do not assume `idf.py test` is host-only or on-target. Do NOT modify any source file as a result of build-error remediation in this task.

**Acceptance criteria**:
- Verify-phase artifact contains the verbatim output of `cd firmware/gateway && idf.py build` and `cd firmware/gateway/test && idf.py test`, OR a plain-language record that each command was not executed because the toolchain is not installed.
- The record explicitly distinguishes (a) build/tests succeeded, (b) build/tests failed with error verbatim, (c) toolchain unavailable, (d) target unreachable.
- At least the test cases for the five new components are accounted for in the test record: `[espnow_manager]`, `[http_server]`, `[mqtt_bridge]`, `[ota_manager]`, `[display_manager]`.
- No claim of on-target runtime behavior beyond what the captured output actually shows.

### T-01.08 — Physical-board boot smoke after NVS bootstrap

- [ ] T-01.08 — Physical-board boot smoke after NVS bootstrap.

**Spec requirements**: "Physical-Board Boot Smoke After NVS Bootstrap" — when the physical board is reachable, capture serial-monitor output proving NVS-first ordering; when unreachable, record the smoke as pending evidence and do NOT declare the issue fully verified.

**Files affected**: None (evidence-only; no source file edits).

**Dependencies**: T-00.01, T-01.07 (the gateway build must succeed before flashing it; the boot smoke is a separate evidence task against the flashed binary, not against `idf.py test`).

**Check command** (caller-supplied `PORT`, never hard-coded): from `firmware/gateway`, run `idf.py -p "$PORT" flash monitor` (or `idf.py -p "$PORT" flash` followed by `idf.py -p "$PORT" monitor` if separate capture is needed). Save the raw captured serial output.

**Description**: Discover the target's actual serial device on the verify host (do not hard-code `/dev/ttyACM0` or any other device). Set a caller-supplied `PORT` variable to that value. Flash the gateway app and capture the serial-monitor output. Verify the captured output contains an NVS-initialization log line emitted by the source-local `nvs_config` tag, then contains the subsequent `board_profile` log line, with the NVS log line appearing earlier than the board-profile log line. Record the verbatim captured output separately from T-01.07 build/test evidence. If the physical board is not reachable, record the boot smoke as pending evidence with the reason and explicitly state that the issue is not fully verified: the build/test evidence from T-01.07 stands, but the minimal boot-acceptance criterion is not satisfied until the pending smoke is captured.

**Acceptance criteria**:
- Verify-phase artifact contains the verbatim captured serial-monitor output, separated from T-01.07 evidence, OR a plain-language pending-evidence record stating the reason.
- When captured, the output contains an `nvs_config` NVS-success marker and a `board_profile` marker, with the `nvs_config` line appearing first.
- When pending, the artifact explicitly states that the issue is not fully verified and identifies the blocker (toolchain not installed / no target port reachable).
- The task does NOT touch any source file.

---

## Rollback

Rollback is the inverse of apply. Each task can be reverted independently; later tasks depend on earlier ones (T-01.06 depends on T-01.01 … T-01.05 test files existing; T-01.07 depends on T-01.06; T-01.08 depends on T-01.07).

1. **Reverse-order revert per chained PR** — `git revert <merge-commit>` for each merged slice in reverse dependency order: T-01.08 → T-01.07 → T-01.06 → T-01.05 → T-01.04 → T-01.03 → T-01.02 → T-01.01 → T-00.01. Each revert removes only that slice's files and edits.
2. **Single-shot revert** — `git revert <merge-commit>` for the single merge commit when the change is delivered as one PR; drops the new component directories, the `nvs_config` files, the `main.c` / `main/CMakeLists.txt` edits, and the `test/CMakeLists.txt` `TEST_COMPONENTS` entries.
3. **Verify revert** — run `cd firmware/gateway/test && idf.py test` against the reverted tree to confirm the original `board_profile;board_rgb` test set still passes when the toolchain is available; record any toolchain unavailability. The revert must not modify any NVS key, register any peer, start any service, or allocate persistent state.
4. **Do not clean the dirty worktree** — if the change was opened against the dirty `chore/sdd-archive-iiot-kit-transformation` worktree, do not `git reset --hard` or `git clean` to recover; planning recommends worktree isolation as a delivery best practice, but the worktree decision belongs to a later parent/user delivery call.

---

## Out of scope (recap)

No tasks were generated for any of the following. Apply phase rejects any wave that touches them.

- `wifi_manager` component, `wifi_manager_init()` call, `wifi_manager` test, `wifi_manager` registration. Owned by issue #21 / `odd/tasks/issue-21-s3-apsta.md`.
- `actuator_ctrl` component. Not in issue #9 body.
- `firmware/common/protocol/`, `firmware/node/`, `firmware/gateway/web/`.
- NVS typed accessors. Fase 1 T1.2.3 owner.
- Kconfig / Kconfig.projbuild files for any new component.
- Full runtime behavior for any component (AP+STA bring-up, ESP-NOW peer registration, REST endpoints, WebSocket frames, SPIFFS asset serving, MQTT publish/subscribe, OTA partition rotation, ST7789 driver, NVS typed accessors, sensor drivers, SNTP, watchdog tuning).
- Documentation edits to `CLAUDE.md`, `project.md`, `Fases/*`, `docs/replanificacion/*`, `docs/hardware/*`, `docs_site/*`, `Tutorial/*`, `knowledge/*`.
- Edits to `firmware/gateway/CMakeLists.txt`, `firmware/gateway/sdkconfig.defaults` (tracked source — out of scope, but NOT a generated output), `firmware/gateway/partitions.csv`, `components/board_profile/**`, `components/board_rgb/**`, `test/main/**`, or existing `components/board_*/test/**`.
- Commit, push, PR creation, worktree creation, `git clean`, `git reset --hard`, or `git stash`. Remain a later user/parent decision.
- Build/test remediation edits if a build or test fails. T-01.07 records failures; it does not remediate them.
