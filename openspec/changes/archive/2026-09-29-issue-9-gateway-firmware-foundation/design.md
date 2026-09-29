# Design: Gateway Firmware Foundation (Issue #9)

## Status and scope

This design implements the proposal and `gateway-firmware-foundation` spec as contract-only C surfaces plus NVS bootstrap composition. Runtime networking, serving, broker, OTA, display, and typed-NVS behavior remain owned by later changes. No `wifi_manager` file is created, modified, tested, deleted, or used as a dependency; issue #21 owns that surface even if it exists by apply time.

Only change-local OpenSpec artifacts are modified during this design phase. Apply may add the proposal-authorized gateway source/component/test files; it must not touch documentation, existing component implementations, or unrelated changes.

## Repository evidence and conventions

Inspected existing patterns:

- `firmware/gateway/components/board_profile/CMakeLists.txt`, `include/board_profile.h`, `board_profile.c`, and its test CMake/source establish ESP-IDF component registration, public `esp_err_t` headers, and component-local Unity tests.
- `firmware/gateway/components/board_rgb/` confirms the same `include/` and `test/` layout, source-local `ESP_LOG` tags, and `PRIV_REQUIRES unity` test registration.
- `firmware/gateway/main/main.c` currently calls `board_profile_read()` first and logs with `gateway`; `main/CMakeLists.txt` registers only `main.c` and requires `board_profile`.
- `firmware/gateway/test/CMakeLists.txt` sets `EXTRA_COMPONENT_DIRS` to `../components`, initializes cached `TEST_COMPONENTS` with `board_profile;board_rgb`, and uses `list(FIND ...)` to append `board_rgb` defensively. `test/main/CMakeLists.txt` registers the test runner.
- Root gateway CMake uses the ESP-IDF project CMake include; no extra test framework or project-wide build wrapper is present.

Use the established `#pragma once`, `esp_err.h`, component-local `TAG`, C11 naming, component `CMakeLists.txt`, and Unity `TEST_CASE` conventions. Do not add an ESP-IDF version guard: the existing components have none, and the spec explicitly avoids requiring one.

## Public API decisions

Keep APIs void-argument, minimal, and `esp_err_t`-returning. No configuration structs, callback types, speculative enums, or options are needed for this contract layer.

| Surface | Public declaration | Constant | Rationale / contract |
|---|---|---|---|
| `nvs_config` (in `main/`) | `esp_err_t nvs_config_init(void);` | None | Bootstrap only; namespace names/default are documentation, not typed API. |
| `espnow_manager` | `esp_err_t espnow_manager_init(void);` | `ESPNOW_MANAGER_MAX_PEERS 20U` | Spec fixes the peer-table maximum; no peer APIs or radio operation. |
| `http_server` | `esp_err_t http_server_start(void);` and `esp_err_t http_server_stop(void);` | None | The required lifecycle seam only; no server handle/state contract. |
| `mqtt_bridge` | `esp_err_t mqtt_bridge_init(void);` | None | No client handles/configuration; MQTT component is not linked or initialized. |
| `ota_manager` | `esp_err_t ota_manager_init(void);` | None | No partition or update interface is specified. |
| `display_manager` | `esp_err_t display_manager_init(void);` | `DISPLAY_MANAGER_FRAMEBUFFER_SIZE_BYTES (170U * 320U * 2U)` | Records documented geometry budget only; no allocation/rendering API. |

Each function gets a brief Doxygen description and an ownership note with the spec wording: ESP-NOW Fase 1 T1.4.x; HTTP Fase 4; MQTT Fase 1 T1.5.x plus Kconfig default-off ownership; OTA Fase 5 T5.x; display future wiring change gated on physical verification. Implementations and tests may return/expect `ESP_OK` because these are inert placeholders, but this is **not** a promise that future runtime initialization can succeed unconditionally. The real owners may replace placeholder internals while preserving only the agreed minimal entry-point signatures/constants.

## Behavior and data flow

### NVS bootstrap

`app_main()` calls `nvs_config_init()` before declaring/reading board profile or invoking any other initializer. On non-`ESP_OK`, it logs an abort message with `gateway` and returns immediately. `nvs_config_init()` owns detailed NVS logging under its source-local `nvs_config` tag, including the failure code/name; this avoids exporting a logging tag across translation units.

Algorithm:

1. Call `nvs_flash_init()`.
2. If result is `ESP_ERR_NVS_NO_FREE_PAGES` or `ESP_ERR_NVS_NEW_VERSION_FOUND`, call `nvs_flash_erase()` once.
3. If erase fails, log and return that error; do not retry initialization after a failed erase.
4. Retry `nvs_flash_init()` once after a successful erase.
5. Return `ESP_OK` on success; otherwise log and return the final initialization error. This is the standard recoverable erase-and-retry path, not an unbounded loop.

Successful init emits an info-level `nvs_config` log, providing the observable NVS marker required by the physical boot smoke. Then `board_profile_read()` proceeds as it does today. There is no NVS namespace opening, key read/write, migration, or typed accessor.

Document in `nvs_config.h` the namespace table (`wifi`, `mqtt`, `ota`, `display`, `node`) and `mqtt_namespace = "iiot-kit"`, citing `docs/replanificacion/02-protocolo-unificado.md` as the source of that default. This is comment-only documentation, not new behavior.

### Contract-only modules

Each module source defines a private, module-matching `TAG`, writes a concise `ESP_LOGI` placeholder message when called, and returns `ESP_OK`. No allocations, FreeRTOS task creation, sockets, radio operations, peripheral initialization, or external state are introduced. The HTTP `stop` stub is equally inert. No stub claims to have started or stopped a real service; wording should explicitly say interface/placeholder only.

`mqtt_bridge` includes no MQTT headers and its component has no `mqtt` dependency. There is no Kconfig/Kconfig.projbuild. `display_manager` only exposes the framebuffer byte-count macro; it does not reserve or allocate that memory.

### Display hardware hold

`display_manager.h` must include this hardware hold verbatim, without paraphrase or omission:

> - GPIO35, GPIO36, and GPIO37 are consumed by the N16R8 Octal PSRAM and are unavailable for external use.
> - GPIO48 is reserved for the onboard WS2812 RGB LED.
> - GPIO19 and GPIO20 are reserved for native USB while USB is needed.
> - The withdrawn seller value of `0.1155 × 0.1155 mm` display pixel pitch is incorrect and MUST NOT be used. Geometry derived from that pitch MUST NOT be inferred, and no replacement pitch SHALL be invented.
> - The 8-pin display's GPIO mapping (RES, DC, CS, SCL, SDA, BLK), backlight current/control, reset polarity, and logic input levels remain unresolved.
> - The RGB565 framebuffer size `170 × 320 × 2 = 108,800` bytes (≈106.25 KiB) is fixed by the documented module facts and SHALL be reflected as a header constant.

No GPIO assignment, SPI setup, ST7789 command, backlight/electrical assumption, or inference from seller information is permitted. The design does not use the withdrawn pitch or derive geometry from it.

## Component/file layout and exact CMake dependencies

For each of `espnow_manager`, `http_server`, `mqtt_bridge`, `ota_manager`, and `display_manager`, use:

```text
firmware/gateway/components/<name>/
  CMakeLists.txt
  <name>.c
  include/<name>.h
  test/CMakeLists.txt
  test/test_<name>.c
```

Component registration:

```cmake
idf_component_register(SRCS "<name>.c"
                    INCLUDE_DIRS "include"
                    REQUIRES esp_common log)
```

`esp_common` supplies the `esp_err.h` contract; `log` is explicit for source-local `ESP_LOGI`. No module requires Wi-Fi, ESP-NOW, HTTP server, MQTT, OTA, SPI, GPIO, RMT, FreeRTOS, or display-driver components for inert stubs. In particular `mqtt_bridge` MUST NOT list `mqtt`.

Each test CMake follows the repository pattern:

```cmake
idf_component_register(SRCS "test_<name>.c"
                    INCLUDE_DIRS "."
                    REQUIRES <name>
                    PRIV_REQUIRES unity)
```

`nvs_config.c` is registered in existing `main/CMakeLists.txt` `SRCS`; add `nvs_flash` and `log` to `REQUIRES` (retain `board_profile`). `nvs_flash` supplies the initialization/erase API and `log` provides explicit log dependency. `main.c` includes `nvs_config.h`, calls it first, then performs the existing board profile read. Do not add new direct Wi-Fi dependencies.

In `firmware/gateway/test/CMakeLists.txt`, preserve its current `EXTRA_COMPONENT_DIRS`, cached default list, existing `board_rgb` `list(FIND ...)` check, and project setup. Append the five contract components defensively, one `list(FIND TEST_COMPONENTS "<name>" ...)` / conditional `list(APPEND ...)` per name (or equivalent loop retaining the same duplicate-avoidance semantics). Do not change the order of `board_profile;board_rgb`; append in order `espnow_manager;http_server;mqtt_bridge;ota_manager;display_manager`. Do not register `nvs_config` as a component: it lives in app `main/` and is exercised through application build/boot evidence, not the per-component test runner.

## Unity tests and evidence boundaries

Each new component test contains at least one `TEST_CASE` tagged exactly `[<name>]`. The test invokes the safe placeholder entry point(s) and asserts `ESP_OK`, proving the symbols compile/link into the test binary and presently have inert placeholder return behavior. `display_manager` also asserts `DISPLAY_MANAGER_FRAMEBUFFER_SIZE_BYTES == 170U * 320U * 2U`. No test expects runtime feature behavior, drives hardware, opens a socket, starts a task, or manipulates NVS. The tests do not call NVS bootstrap since its destructive recovery path must not be exercised as an ordinary Unity contract check.

Evidence has separate categories:

1. **Gateway app build** — run exactly `cd firmware/gateway && idf.py build`; records compilation/link of app and dependencies only, not a boot.
2. **Gateway Unity project** — run exactly `cd firmware/gateway/test && idf.py test`; record the command's actual output and whether it compiled, linked, flashed, monitored, or otherwise ran tests. Do not assume `idf.py test` is host-only or on-target; report observed steps only.
3. **Physical boot-monitor smoke** — separately flash the gateway app to the physical ESP32-S3-WROOM-1-N16R8 (or the documented equivalent) and capture monitor output. Discover the correct port on the host (for example, inspect the system's enumerated serial devices / ESP-IDF port listing); set a caller-supplied `PORT` variable, never hard-code `/dev/ttyACM0` or another device. Reproducible procedure:
   - Source the installed ESP-IDF environment and ensure the gateway app is configured/built for `esp32s3`.
   - Identify the target's actual serial device and set `PORT` to that value.
   - From `firmware/gateway`, run `idf.py -p "$PORT" flash monitor` (or `idf.py -p "$PORT" flash` followed by `idf.py -p "$PORT" monitor` if separate capture is needed).
   - Save the raw captured serial output. Verify it contains an `nvs_config` NVS-success marker before the first `board_profile` log. Preserve the exact output and distinguish missing device/tooling from a failed boot or missing marker.

A successful app build and/or test build is not physical boot evidence. If the board/port is unavailable, report the boot smoke as pending evidence; the issue's minimal boot-acceptance criterion remains unverified. If NVS init fails, the `nvs_config` diagnostic plus absence of later board-profile logs is the intended safe halt, not a pass for the success-order smoke.

## Future flow boundaries (context only; no runtime designed here)

The repository design rules request flow diagrams. These show where the later owning implementations participate; they do not add APIs, runtime behavior, or a dependency on `wifi_manager` to this contract change.

ESP-NOW sensor-data flow (implemented by later owner work):

```text
ESP32-C3 node -> ESP-NOW radio -> gateway espnow_manager -> future data handling
                                  (contract stub here; no radio init/receive/send)
```

OTA flow (later OTA and HTTP implementation):

```text
Browser/client -> future http_server upload route -> future ota_manager -> OTA partition
                  (no route/socket here)          (no OTA APIs here)
```

WebSocket event flow (later HTTP/data implementation):

```text
Browser <-WebSocket-> future http_server <- future sensor/data publisher
                      (no socket/handler here)
```

SPIFFS-served SPA flow (later HTTP/dashboard implementation):

```text
Browser -> future http_server -> SPIFFS asset -> Browser
           (no HTTP server or SPIFFS mount here)
```

These are ownership boundaries, not sequence guarantees for this foundation. Their interactions and runtime sequencing remain for the relevant implementation changes.

## Memory and power impact

All five runtime skeletons are bounded to code, a static log-tag pointer, and transient log formatting controlled by ESP-IDF logging. They create no large allocations, PSRAM use, task, socket, radio, timer, or peripheral state. `display_manager` carries only an integer compile-time framebuffer size constant (108,800 bytes); it allocates **zero** framebuffer bytes. The NVS bootstrap invokes ESP-IDF NVS initialization only and introduces no application buffers or key data. No battery-node path is changed; power impact is therefore limited to normal boot-time NVS initialization and the negligible logging/code overhead, with no ongoing radio/display power draw attributable to these skeletons. This is a design estimate, not measured runtime evidence.

## Sequencing, rollout, and rollback

Recommended apply ordering:

1. **Wave A (NVS)**: add `nvs_config.{c,h}`, register its source/dependencies, and put its checked call first in `app_main()`. Build and inspect the diff. This must land before issue #21's `main.c` wiring.
2. **Waves B–F (components)**: add each component's source/header/CMake and Unity test. Each is isolated and has no cross-component dependencies; no wave touches `wifi_manager`.
3. **Wave G (test registration)**: append the five names to `TEST_COMPONENTS` in the same delivery slice as the new test components.
4. Build/test evidence; physical boot smoke separately where hardware is available.

Issue #21 may add `wifi_manager` after this change, but issue #9 has no dependency on it. Preserve final `app_main()` order as NVS init, board profile read, then any later issue #21 Wi-Fi startup. Keep issue #9's additions to shared `main/CMakeLists.txt` and test CMake minimal and independently mergeable. Guard each apply diff against any path matching `**/wifi_manager/**`, documentation, and all out-of-scope paths listed in the proposal/spec.

Rollback is a revert of the issue #9 change (or reverse-order reverts when independently released slices depend on earlier work), removing the NVS call and source registration, newly added contract component dirs, and the five test-list entries. No migration/cleanup is needed: this layer writes no keys, registers no peers, starts no services, and allocates no persistent state. Never reset/clean/stage unrelated worktree contents as part of rollback.

## Review slices and decision gates

Proposal forecast is approximately 630 changed lines across about 28 files, above the 400-line review budget. Keep slices narrow: A (NVS), component contract slices (B–F; combine only if the measured diff stays reviewable), and test registration with the components it enables. The apply/orchestration phase must stop and ask under `ask-on-risk` before selecting a delivery strategy when the aggregate or a proposed slice crosses the review budget. This design does not select a chain strategy or grant `size:exception`.

For each slice review: public surface minimality, exact dependency list, no runtime behavior, module-local log tags, Unity tag/symbol evidence, and paths touched. For all slices also review NVS-first failure halt, hardware-hold verbatim, no MQTT link/Kconfig, no `wifi_manager` paths/dependency, and proper separation of compile/test from physical boot evidence.

## Risks and mitigations

| Risk | Mitigation |
|---|---|
| Issue #21 modifies the same `app_main()` and CMake/test registration surfaces. | Land Wave A first; issue #21 appends its init only after successful NVS and board-profile ordering; review both shared-file diffs at integration. |
| Placeholder `ESP_OK` is misread as service readiness. | Log explicit contract-only/unimplemented wording; tests only establish current placeholder contract and linker visibility. Do not log “started”, “connected”, or “ready”. |
| NVS erase path destroys NVS contents. | Erase only on the two documented recoverable return codes; do not erase on other failures; disclose recovery behavior in header/source comments and capture actual boot evidence. |
| Display specifications tempt implementation assumptions. | Preserve the exact hardware hold verbatim; no pins, SPI, commands, driver, or backlight behavior. Framebuffer constant is not allocation. |
| MQTT dependency leaks into skeleton. | No MQTT include, symbol, Kconfig, or `mqtt` CMake requirement; inspect dependency and link output in review/build. |
| Test/build result is overstated as physical behavior. | Record each command's observed actions and boot-monitor transcript independently; missing board means pending smoke, not verified. |
| Existing worktree contains unrelated changes. | Use the parent-selected clean worktree/branch at apply; never stage, clean, reset, or modify unrelated dirty/untracked paths. |

## Decisions deferred

No callback contracts, settings structs, broker/topic/URI abstractions, partition-label enums, SPI configuration, status getters, lifecycle states, typed NVS APIs, or performance budgets are established. Owners should add only surfaces required by their runtime specs and retain these minimal entry points where compatible. No physical board behavior, display wiring, power consumption, NVS layout migration, or runtime resource footprint is claimed by this design.
