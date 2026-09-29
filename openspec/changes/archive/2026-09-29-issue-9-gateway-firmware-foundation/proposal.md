# Proposal: Gateway ESP-IDF Firmware Foundation (Issue #9)

## Intent

Close the **foundation-only contract gap** in `firmware/gateway/` that issue #9 names but the merged issue #8 / PR #17 work did not yet deliver. Issue #8 merged a compilable scaffold (`firmware/gateway/CMakeLists.txt`, `main/`, `sdkconfig.defaults`, `partitions.csv`), two narrow components (`board_profile`, `board_rgb`), and a Unity test runner. The remaining deliverables named in issue #9 — a small NVS bootstrap composition, contract/skeleton foundations for five additional components, and per-component Unity test skeletons — are still genuinely unimplemented.

This change ships **contracts and skeletons only**. Full runtime behavior (AP+STA bring-up, ESP-NOW peer registration, REST/WebSocket routes, MQTT publish/subscribe, OTA partition rotation, ST7789 driver, NVS typed accessors) belongs to its owning change.

## Why now

Issue #9 is approved and the issue body explicitly enumerates this work. The repository is at Fase 0 / Fase 1 foundation: a scaffold exists, but the next modular layer needed by Fase 1 sub-tasks (T1.1.x, T1.2.x, T1.3.x, T1.4.x, T1.5.x) and later phases (T5.x OTA, display change) is not yet in the tree. Shipping the contracts now gives downstream implementers a stable seam to call into without redefining it in the same commit.

## Scope

### In scope (deliverable in this change)

1. **NVS bootstrap composition** — small `nvs_config_init()` foundation placed in `main/` per Fase 1 T1.1.1's existing convention:
   - `firmware/gateway/main/nvs_config.h` — declares `nvs_config_init()` and documents the NVS namespace table (`"wifi"`, `"mqtt"`, `"ota"`, `"display"`, `"node"`) plus the documented default key `mqtt_namespace = "iiot-kit"` (source of truth: `docs/replanificacion/02-protocolo-unificado.md`). No typed accessors — those belong to Fase 1 T1.2.3.
   - `firmware/gateway/main/nvs_config.c` — thin wrapper around `nvs_flash_init()` with the recoverable-erase pattern (`ESP_ERR_NVS_NO_FREE_PAGES` / `ESP_ERR_NVS_NEW_VERSION_FOUND` → `nvs_flash_erase()` → retry) and a module log tag.
   - `firmware/gateway/main/main.c` — minimal edit: call `nvs_config_init()` as the **first** operation in `app_main()` and **handle its failure before** any subsequent step (`board_profile_read()` or future component init). The current `board_profile_read()` call remains, sequenced after a successful NVS init.
   - `firmware/gateway/main/CMakeLists.txt` — register `nvs_config.c` in `SRCS` and add `nvs_flash` to `REQUIRES`.

2. **Five component contract/skeleton foundations**, each mirroring the `board_profile` layout (CMakeLists.txt, `include/<name>.h`, `<name>.c` skeleton, `test/CMakeLists.txt`, `test/test_<name>.c`):

   - `espnow_manager` — public init entry point declared in the header; skeleton returns success and logs a documented placeholder. The peer-table size of `20` is fixed by ESP-NOW protocol limits documented in `CLAUDE.md` ("Max 20 encrypted peers") and reflected as a header constant.
   - `http_server` — public start/stop entry points declared in the header; skeleton returns success and logs a documented placeholder.
   - `mqtt_bridge` — public init entry point declared in the header; skeleton compiles without linking or initializing the ESP-IDF MQTT client; exposes no runtime behavior. No Kconfig file is introduced in this change.
   - `ota_manager` — public init entry point declared in the header; skeleton returns success and logs a documented placeholder.
   - `display_manager` — public init entry point declared in the header; the RGB565 framebuffer size constant `170 × 320 × 2 = 108,800` bytes is fixed by documented module facts in `CLAUDE.md` and reflected as a header constant. Skeleton returns success and logs a documented placeholder.

3. **Unity contract tests** — one or more `TEST_CASE` entries per component asserting that the declared public symbols exist (linker-level evidence). Tests use the same `[<module>]` Unity tags already used by `board_profile` and `board_rgb`. Exact test bodies, signature assertions, and any constant-value checks belong to the spec/design phases.

4. **TEST_COMPONENTS registration** — `firmware/gateway/test/CMakeLists.txt` appends the five new components alongside the existing `board_profile;board_rgb` entry. The file's defensive `list(FIND ...)` pattern is preserved.

5. **Cross-cutting conventions** in every new header — `#pragma once` guard, ESP-IDF version guard, `esp_log` with module-specific tag, `esp_err_t` returns, snake_case naming, brief Doxygen block on each public function noting "Interface only — full implementation owned by change Y". Exact public function signatures, enums, and structs belong to the spec/design phases, not to this proposal.

### Out of scope (explicit)

- **`wifi_manager`** — wholly owned by issue #21 / `odd/tasks/issue-21-s3-apsta.md`. Authoritative user decision: this change does not create, edit, test, delete, or depend on any `wifi_manager` files. The directory may already exist by apply time because issue #21 owns it; that presence does not block this change. The invariant is only that no wave of this change touches any `wifi_manager` file.
- **`actuator_ctrl`** — listed in `CLAUDE.md` § Project Structure but absent from issue #9's explicit component list. Out of scope; if/when needed, gets its own issue/change.
- **`firmware/common/protocol/`** — shared protocol component. Related dependency / future foundation item, not part of issue #9.
- **`firmware/node/`** — ESP32-C3 node firmware. Not in scope.
- **`firmware/gateway/web/`** — Preact SPA. Belongs to Fase 4.
- **Full runtime for any component** — AP+STA bring-up, ESP-NOW peer registration / ACK retry / whitelist, REST endpoints, WebSocket frames, SPIFFS asset serving, MQTT publish/subscribe lifecycle, broker connection state machine, full `esp_ota_*` flow, partition selection / rollback, ST7789 register writes / GPIO assignment / backlight driver / DMA, SNTP, watchdog tuning, sensor drivers, typed NVS accessors, Kconfig option files for the new components.
- **Most merged issue #8 surfaces** — `firmware/gateway/CMakeLists.txt`, `sdkconfig.defaults`, `partitions.csv`, `components/board_profile/`, `components/board_rgb/`, `test/main/`, `components/board_*/test/`, and any docs (`CLAUDE.md`, `project.md`, `Fases/*`, `docs/replanificacion/*`, `docs/hardware/*`, `docs_site/*`, `Tutorial/*`, `knowledge/*`) remain untouched. The only **existing source files** this change may modify are `firmware/gateway/main/main.c` (one NVS init call), `firmware/gateway/main/CMakeLists.txt` (one SRCS / REQUIRES edit), and `firmware/gateway/test/CMakeLists.txt` (one TEST_COMPONENTS append).
- **Touching the dirty worktree** — branch `chore/sdd-archive-iiot-kit-transformation` carries unrelated dirty and untracked paths. Do not clean, stage, commit, or touch them. Implementation waves operate from a separate worktree/branch created at apply time.

## Approach

Sequenced waves, each independent and reversible. Each wave's full scope is bounded; the **aggregate** change will exceed the 400-line review budget across five components plus NVS init — apply phase must use bounded slices (chain strategy deferred until chaining is selected per session preflight `delivery_strategy: ask-on-risk`).

| Wave | Surface | Files (approx.) | Lines (approx.) |
|---|---|---|---|
| **A — NVS init** | `main/nvs_config.{c,h}` (new) + `main/main.c` (edit) + `main/CMakeLists.txt` (edit) | 2 new + 2 edits = 4 | ~70 |
| **B — `espnow_manager`** | component dir (CMakeLists.txt + include/header + source + test/CMakeLists.txt + test/test_source) | 5 new | ~110 |
| **C — `http_server`** | component dir | 5 new | ~110 |
| **D — `mqtt_bridge`** | component dir (no Kconfig, no MQTT client link) | 5 new | ~100 |
| **E — `ota_manager`** | component dir | 5 new | ~110 |
| **F — `display_manager`** | component dir (hardware hold) | 5 new | ~120 |
| **G — TEST_COMPONENTS registration** | `test/CMakeLists.txt` | 1 edit | ~10 |
| **Total forecast** | | **~28** | **~630** |

This is a conservative forecast: each component header and skeleton is ~15–25 lines; each Unity test source is ~15–25 lines; each `test/CMakeLists.txt` is ~3 lines; each top-level `CMakeLists.txt` is ~3 lines. Headers and skeletons include `#pragma once`, ESP-IDF version guard, Doxygen block, log tag, init function declaration, init function body returning `ESP_OK` with a placeholder log, and a Unity test asserting the symbol exists. Wave A is the natural foundation commit (smallest, isolated, sets the bootstrap ordering convention used by every later wave). Waves B–F are independent of each other and may be reordered per apply-phase decision. Wave G is the test runner edit that registers them all; it must land in the same apply phase as the new components so `idf.py test` can build them. The review-budget forecast above triggers the apply-phase ask-on-risk pause; **chain strategy selection is deferred** to apply, not made here.

## Capabilities

### New capabilities

- **`gateway-firmware-foundation`** — a stable header-only/skeleton seam for `espnow_manager`, `http_server`, `mqtt_bridge`, `ota_manager`, `display_manager`, and `nvs_config_init()`. Downstream Fase 1 / Fase 5 / display changes can implement runtime behavior without redefining the seam.

### Modified capabilities

- None at the SDD layer. The merged issue #8 capabilities (`gateway-board-support`, `gateway-rgb-evidence`) are untouched.

## Phase ordering

Compatible with Fase 0 / Fase 1 foundation work per `openspec/config.yaml` `rules.proposal` ("Keep proposals compatible with the active educational phase ordering (Fases 0–6)"):

- Ships **before** Fase 1 implementation work that needs the seam (T1.1.3 ESP-NOW init, T1.3.x HTTP server, T1.5.x MQTT bridge, T5.x OTA flow) and before any future display wiring change.
- Does not block Fase 0 toolchain / scaffold work — that is already merged via issue #8 / PR #17.
- Does not promote Fase 4 SPA, Fase 6 Sparkplug B, or any later-phase content.

## Affected areas (projects)

Per `openspec/config.yaml` `projects[]`:

- **`firmware/gateway`** — single affected project.
  - Test command: `cd firmware/gateway/test && idf.py test`
  - Build command: `cd firmware/gateway && idf.py build`
- `docs_site`, `firmware/node`, `firmware/common`, `firmware/gateway/web` — not touched.

## Acceptance outcomes

Honest framing: contract tests prove **linker-level symbol existence** for the new public init entry points. They do **not** prove runtime behavior. Each component's real acceptance criteria live in its owning change (issue #21 for wifi; Fase 1 T1.4.x for ESP-NOW; Fase 1 T1.3.x for HTTP; Fase 1 T1.5.x for MQTT; Fase 5 T5.x for OTA; display change once hardware is validated).

Outcomes this change commits to:

- `nvs_config_init()` is the first operation in `app_main()`. Its return value is checked; on failure, the function logs the error via `ESP_LOGE` and returns without calling `board_profile_read()` or any other init step.
- `espnow_manager`, `http_server`, `mqtt_bridge`, `ota_manager`, `display_manager` each expose a public init entry point declared in their header and compiled into the gateway test project.
- `mqtt_bridge` skeleton compiles without linking the ESP-IDF MQTT client; it does not call `esp_mqtt_client_init` and exposes no runtime behavior beyond a documented placeholder log.
- The five new components are registered in `firmware/gateway/test/CMakeLists.txt` `TEST_COMPONENTS` alongside `board_profile;board_rgb`.
- Generated outputs (`sdkconfig`, `build/`) remain untracked; new commits do not stage them.

Verification: `cd firmware/gateway/test && idf.py test` and `cd firmware/gateway && idf.py build` are the configured project-local commands. When the hardware/toolchain are available on the host, the verify phase runs them and records the output. When the toolchain is unavailable or the target hardware is not connected (e.g., no `/dev/ttyACM0` reachable, as already documented for issue #21's flow), the verify phase records the unavailability and the on-target test execution evidence is reported as "not collected" rather than skipped silently. The proposal does not promise on-target runtime evidence for the new contracts; the proposal does promise reproducible build/test execution when tooling allows and honest reporting when it does not.

Outcomes **not** proven by this change (explicit):

- WiFi AP+STA bring-up, ESP-NOW peer registration, REST/WebSocket routes, MQTT pub/sub, OTA partition rotation, ST7789 driver, NVS typed accessors, end-to-end sensor data flow, Kconfig default-off verification (no Kconfig file is introduced in this change).

## Risks

| Risk | Likelihood | Impact | Mitigation |
|---|---|---|---|
| **Worktree state** — branch `chore/sdd-archive-iiot-kit-transformation` is dirty with unrelated edits and untracked files. Implementation must not clean, reset, stage, commit, or touch them. | High | Medium | New files written only under `openspec/changes/issue-9-gateway-firmware-foundation/`. Implementation waves operate from a separate clean worktree/branch (or stacked branch) created at apply time; the parent orchestrator owns the worktree decision. Apply phase runs `git status` guard before each wave and rejects any wave that would touch unrelated dirty/untracked paths. No `git add`, `git commit`, or `git clean` against the current worktree's dirty/untracked paths. |
| **`wifi_manager` directory presence at apply time** — issue #21 may already have created `firmware/gateway/components/wifi_manager/` before issue #9 waves run. | High | Low | Issue #9 does not require the directory to be absent; the invariant is only that no wave of this change creates, edits, tests, deletes, or depends on any `wifi_manager` file. Apply-phase guard: each wave's diff must not touch any path matching `**/wifi_manager/**`. |
| **`wifi_manager` merge conflicts in `main/main.c`** — issue #21 appends `wifi_manager_init()` to `app_main()` after issue #9 inserts `nvs_config_init()`. | Medium | Medium | Worktree merge order enforced at apply time: this change's wave A lands first, then issue #21 lands its `main.c` edit. If a conflict occurs on the same `app_main` body, the resolution keeps `nvs_config_init()` first and `wifi_manager_init()` after, with `board_profile_read()` ordering preserved. |
| **Stale active change** `openspec/changes/iiot-kit-transformation/` and its archive `archive/2026-09-27-iiot-kit-transformation/`. | Low | Medium | Per session preflight, this proposal phase **must not modify** either copy. Reference only. |
| **Generated ESP-IDF outputs** (`sdkconfig`, `build/`) accidentally staged. | Medium | Low | New components' CMakeLists.txt do not commit outputs; `.gitignore` already excludes `build/`; `sdkconfig` is not in `.gitignore`, so apply phase runs `git status` checks before each commit. |
| **Component contract diverges from eventual owner change**. | Medium | Medium | Each header includes a Doxygen note: "Interface only — full implementation owned by change Y". Spec phase cross-checks signatures against `Fases/fase-01-gateway-nucleo.md` task descriptions (T1.1.3, T1.3.1, T1.5.x, T5.x). |
| **`display_manager` interface makes unverified hardware commitments**. | Medium | High | `display_manager.h` includes the hardware-hold notice verbatim: GPIO35/36/37 unavailable (N16R8 Octal PSRAM); GPIO48 reserved for onboard RGB LED; GPIO19/20 reserved for native USB while needed; the withdrawn 0.1155 mm pixel pitch is forbidden; the 8-pin display's GPIO mapping (RES/DC/CS/SCL/SDA/BLK), backlight current/control, and logic levels remain unresolved. **No** GPIO assignment, **no** backlight driver assumption, **no** ST7789 register writes. |
| **MQTT module pulled into the build** because of an unintended client link or init. | Low | Medium | The skeleton compiles without linking the ESP-IDF MQTT client and does not call `esp_mqtt_client_init`. No Kconfig file is introduced in this change. Spec phase records this constraint for the owning change to honor when MQTT runtime is added. |
| **Implementation exceeds the 400-line review budget**. | High | Medium | Forecast above: ~630 changed lines across ~28 files. Apply phase will recommend **bounded slices** (single-wave PRs; foundation wave A first). Chain strategy (chained PR / `size:exception` / single PR) **deferred** until chaining is selected per session preflight `delivery_strategy: ask-on-risk`. Apply-phase pauses on review-budget risk before opening PRs. |
| **`nvs_config` placement** (`main/` vs `components/`) inconsistent with the Fase 1 doc. | Low | Low | Fase 1 T1.1.1 explicitly puts `nvs_config.{c,h}` in `main/`. This change honors that placement, registers it in `main/CMakeLists.txt`, and lets the owning change (Fase 1 T1.2.3) promote it to a component if/when typed accessors are added. |
| **Preact SPA scaffolding creep**. | Low | Medium | `firmware/gateway/web/` is explicitly out of scope. Apply-phase guard: parent rejects any wave that touches that directory. |
| **`actuator_ctrl` creep**. | Low | Medium | Not in issue #9's component list. Apply-phase guard: parent rejects any wave that creates `firmware/gateway/components/actuator_ctrl/`. |
| **`firmware/common/protocol/` creep**. | Low | Medium | Not in scope. Apply-phase guard: parent rejects any wave that creates files under `firmware/common/`. |
| **Node firmware creep**. | Low | Medium | Not in scope. Apply-phase guard: parent rejects any wave that creates files under `firmware/node/`. |
| **Verification blocks on missing hardware**. | Medium | Low | When the toolchain is unavailable or no target is connected, the verify phase records the unavailable evidence explicitly rather than claiming success; the proposal no longer promises on-target runtime behavior for the new contracts. |

## Dependencies

- **`firmware/gateway/` scaffold** (issue #8 / PR #17, merged): required to exist before any wave runs. Confirmed on disk.
- **ESP-IDF v5.x toolchain**: already validated per `odd/tasks/issue-8-gateway-board-support.md` and `odd/tasks/issue-21-s3-apsta.md`.
- **Issue #21** (`odd/tasks/issue-21-s3-apsta.md`): no code-level dependency because issue #9 excludes `wifi_manager` from its file scope. Coordination only: apply phase must run this change's wave A (NVS init in `main.c`) before issue #21 reaches WUC3 (gateway app `main.c` wiring) so `wifi_manager_init()` is appended after `nvs_config_init()` in correct order.

## Conflicts

| Conflict | Resolution |
|---|---|
| `firmware/gateway/components/wifi_manager/` directory | Owned by issue #21; may already exist at apply time. Issue #9 does not touch any file under that directory. |
| `firmware/gateway/main/main.c` | Both issue #9 (NVS init) and issue #21 (wifi_manager_init) edit `main.c`. Issue #9 lands first; issue #21 appends `wifi_manager_init()` after the NVS init call. Worktree merge order enforced at apply time. Conflict resolution preserves `nvs_config_init()` first, then `board_profile_read()`, then `wifi_manager_init()`. |
| `firmware/gateway/main/CMakeLists.txt` | Issue #9 adds `nvs_config.c` and `nvs_flash` REQUIRES. Issue #21 adds `wifi_manager` REQUIRES. Distinct lines, mergeable. |
| `firmware/gateway/test/CMakeLists.txt` | Issue #9 appends the five new components; issue #21 appends `wifi_manager`. Both changes append distinct components; mergeable if each lands as a single focused edit. |
| `openspec/changes/iiot-kit-transformation/` (stale active + archive) | Do not modify. This proposal only references them. |
| Worktree dirty state (`chore/sdd-archive-iiot-kit-transformation` branch) | Do not clean, stage, commit, or touch. New work happens in a clean worktree/branch at apply time. |

## Rollback plan

This is a contract-only change — new files only, plus tiny edits to `main/main.c` (one NVS init call), `main/CMakeLists.txt` (one SRCS / REQUIRES entry), and `test/CMakeLists.txt` (one TEST_COMPONENTS append). Rollback is the inverse of apply:

1. `git revert <merge-commit>` (one revert per merged PR if chained) — drops the new component directories, the `nvs_config` files, the `main.c` / `main/CMakeLists.txt` edits, and the `test/CMakeLists.txt` `TEST_COMPONENTS` entries.
2. Re-run `cd firmware/gateway/test && idf.py test` against the reverted tree to confirm the original `board_profile;board_rgb` test set still passes when tooling is available; record any toolchain unavailability.
3. If a wave is rolled back after a later wave has already built on it (e.g., wave C depends on wave A's `nvs_config_init`), rollback in reverse order: F → E → D → C → B → A.
4. No state migration is required because no NVS keys are written, no MQTT state is created, and no peer registration happens.
5. If the user opened this change against the dirty `chore/sdd-archive-iiot-kit-transformation` worktree by accident, do **not** `git reset --hard`; the parent orchestrator owns the worktree decision. Reference the preflight instruction: "do not clean, stage, commit, or touch them".

## Success criteria

- Five new component directories exist under `firmware/gateway/components/`, each with the `board_profile` layout: top-level `CMakeLists.txt`, `include/<name>.h`, `<name>.c`, `test/CMakeLists.txt`, `test/test_<name>.c`.
- `firmware/gateway/main/nvs_config.{c,h}` exist; `main.c` calls `nvs_config_init()` as the **first** operation in `app_main()` and handles a non-OK return before any subsequent step.
- `firmware/gateway/main/CMakeLists.txt` registers `nvs_config.c` in `SRCS` and adds `nvs_flash` to `REQUIRES`.
- `firmware/gateway/test/CMakeLists.txt` `TEST_COMPONENTS` lists the five new components alongside `board_profile;board_rgb`.
- `cd firmware/gateway && idf.py build` compiles the gateway app successfully when the toolchain is available.
- `cd firmware/gateway/test && idf.py test` compiles the test project successfully against the target when the toolchain and target hardware are available; otherwise unavailable-evidence is recorded explicitly.
- No `wifi_manager`, `actuator_ctrl`, `firmware/common/protocol/`, `firmware/node/`, or `firmware/gateway/web/` files are created, edited, tested, deleted, or depended on by this change.
- No docs (`CLAUDE.md`, `project.md`, `Fases/*`, `docs/replanificacion/*`, `docs/hardware/*`, `docs_site/*`, `Tutorial/*`, `knowledge/*`) are modified.
- The hardware-hold notice appears verbatim in `display_manager.h`.
- Generated outputs (`sdkconfig`, `build/`) remain untracked; new commits do not stage them.

## Estimated effort

| Wave | Effort |
|---|---|
| A — NVS init | ~1 hour |
| B — `espnow_manager` contract | ~1.25 hours |
| C — `http_server` contract | ~1.25 hours |
| D — `mqtt_bridge` contract (no client link) | ~1.25 hours |
| E — `ota_manager` contract | ~1.25 hours |
| F — `display_manager` contract (hardware hold) | ~1.5 hours |
| G — TEST_COMPONENTS registration | ~0.25 hour |
| Apply-phase slicing decision + chain-strategy ask | ~0.5 hour (ask-on-risk pause) |
| Verify phase | ~1 hour |
| **Total** | **~9–10 hours** |

## Out of scope (restated)

Full runtime behavior for any component, NVS typed accessors, sensor drivers, SNTP, watchdog tuning, Preact SPA, node firmware, shared protocol component, `actuator_ctrl`, and `wifi_manager` body. Each of these belongs to its own issue or change. Exact public function signatures, enums, and struct members for the new components are deferred to the spec and design phases.