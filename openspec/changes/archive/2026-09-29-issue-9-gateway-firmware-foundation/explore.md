# Exploration: Issue #9 — Gateway ESP-IDF Firmware Foundation

**Change**: issue-9-gateway-firmware-foundation
**Status**: done
**Date**: 2026-09-27
**Issue**: https://github.com/fgjcarlos/ESP32-IIoT-Kit/issues/9 — `[Propuesta]: Crear la base modular del firmware ESP-IDF` (`status:approved`)
**Artifact store**: openspec
**Delivery strategy**: ask-on-risk (inherited from session preflight)

---

## Scope

Reconcile the issue #9 proposal — *create the modular base of the ESP-IDF firmware* — with the repository as it actually exists today. The issue's framing assumes the gateway firmware scaffold is missing. In reality, recent merged work (issue #8 / PR #17) has already produced a compilable `firmware/gateway/` scaffold with two narrow components (`board_profile`, `board_rgb`), a partition budget, and a separate Unity test runner. The explore identifies what is genuinely unimplemented for issue #9, what is already satisfied, what conflicts with parallel issues, and what open hardware facts must stay unresolved.

This is **read-only source exploration**. No firmware, docs, or existing OpenSpec artifacts are modified; only `openspec/changes/issue-9-gateway-firmware-foundation/explore.md` and `state.yaml` are written.

## Key Findings

### What the issue #9 proposal explicitly asks for

The issue body enumerates an **initial modular foundation**, not full feature implementations. It calls for:

- **Six component contracts / skeletons** under `firmware/gateway/components/`:
  1. `wifi_manager` — AP + STA lifecycle skeleton
  2. `espnow_manager` — ESP-NOW backbone skeleton
  3. `http_server` — REST + WebSocket server skeleton
  4. `mqtt_bridge` — optional MQTT bridge skeleton
  5. `ota_manager` — OTA manager skeleton
  6. `display_manager` — external LCD interface skeleton (interface only; no GPIO assignment, backlight assumptions, or display implementation)
- **NVS initialization** as part of the bootstrap composition.
- **Logging, versioning, and error handling** as cross-cutting conventions baked into each component skeleton.
- **Per-component Unity test skeletons** mirroring the existing `board_profile` / `board_rgb` pattern.

Each component must compile, expose its public header, declare its initialization entry point, and have a minimal Unity test file. The issue is intentionally **foundation-only**: it does not request full REST/WebSocket routes, full SPIFFS asset serving, runtime MQTT publish/subscribe flows, full OTA partition-rotation flows, full ST7789 driver behavior, the Preact SPA, the `firmware/node/` firmware, the shared `firmware/common/protocol/` component, or other Fase 1–5 behavior. Those are separate changes.

### What already exists on disk (merged from issue #8 — credit, do not duplicate)

| Surface | Status | Source |
|---|---|---|
| `firmware/gateway/CMakeLists.txt` | Compiles | ODD-2 merged via PR #17 (commit `bc31856c`) |
| `firmware/gateway/main/{CMakeLists.txt, main.c}` | Compiles, calls `board_profile_read()` | ODD-2 |
| `firmware/gateway/sdkconfig.defaults` | `esp32s3`, 16 MB flash, Octal PSRAM, custom partition mode | ODD-2/ODD-3 |
| `firmware/gateway/partitions.csv` | `nvs`, `otadata`, `phy_init`, `ota_0` 3 MiB, `ota_1` 3 MiB, `spiffs` 4 MiB | ODD-3 (`87b17d8e`) |
| `firmware/gateway/components/board_profile/` (CMakeLists, header, source, test) | GREEN on target | ODD-2 strict TDD |
| `firmware/gateway/components/board_rgb/` (CMakeLists, header, source, test) | GREEN on target; visual RGB smoke confirmed by user | ODD-4 (`b3823063`) |
| `firmware/gateway/test/` (CMakeLists, `TEST_COMPONENTS=board_profile;board_rgb`, `main/test_main.c`) | Unity runner, factory partition layout | ODD-2 |
| `docs/hardware/esp32-s3-wroom-1-n16r8.md` | Module facts, ODD-3 budget, RGB evidence recorded | ODD-1/3/4 |
| `project.md` § Especificaciones Técnicas | Partition math + measured evidence aligned | ODD-3 |
| `CLAUDE.md` Architecture + Coding Conventions | Aligned with autonomous ESP32-S3 gateway | ODD-2 |
| `openspec/config.yaml` | `firmware/gateway` project-local test command: `cd firmware/gateway/test && idf.py test`; `strict_tdd: false` (project-local Unity only) | Existing config |

Generated ESP-IDF outputs (`sdkconfig`, `build/`) remain local/untracked per ODD-2/3 user authorization and `openspec/config.yaml` notes. Build dirs are ignored; sdkconfigs are not.

### What issue #9's explicit scope STILL requires (genuinely unimplemented)

For each item below the deliverable is a **contract/skeleton** (header + skeleton source + minimal test), not a full implementation:

1. `firmware/gateway/components/wifi_manager/` — initial contract only. Defines the public `wifi_manager_init()` API, mode enum, default SSID/password names, event-handler signatures. **No AP+STA bring-up code in this change** — full lifecycle is owned by issue #21 (`odd/tasks/issue-21-s3-apsta.md`). This change may define the **non-overlapping interface seam** (function signatures only) so issue #21 can fill the body later.
2. `firmware/gateway/components/espnow_manager/` — initial contract only. `espnow_manager_init()` API, recv-callback signature, peer-table max-size constant (≤20 per protocol docs), channel-log helper signature. No runtime peer registration or full ACK flow.
3. `firmware/gateway/components/http_server/` — initial contract only. `http_server_start()`/`http_server_stop()` API, default port constant, URI-handler registration signature. No REST routes or WebSocket frames in this change.
4. `firmware/gateway/components/mqtt_bridge/` — initial contract only (optional module, gated by Kconfig default off). `mqtt_bridge_init()` API, namespace-getter signature, broker URL struct. No runtime publish/subscribe.
5. `firmware/gateway/components/ota_manager/` — initial contract only. `ota_manager_init()` API, partition-label enum, version-getter signature. No `esp_ota_*` flow.
6. `firmware/gateway/components/display_manager/` — initial contract only (interface only). `display_manager_init()` API, framebuffer-size constant (`170 × 320 × 2 = 108,800 bytes` per the documented module facts), SPI-mode enum, **no GPIO assignment**, **no backlight driver assumption**, **no ST7789 register writes**. Hardware hold: see the open constraints section below.
7. NVS initialization as the first step in `app_main` (per Fase 1 T1.1.1) — currently absent. The change adds `nvs_config_init()` to `main.c` ordering before any component init call, with documented NVS namespaces and default keys. The implementation can be a thin wrapper around `nvs_flash_init()` + recoverable erase, not a full typed-accessor module (full accessors belong to Fase 1 T1.2.3 and are out of issue #9 scope).
8. Per-component Unity test skeletons registered in `firmware/gateway/test/CMakeLists.txt` `TEST_COMPONENTS` (mirroring the existing `board_profile;board_rgb` entry).
9. Cross-cutting conventions in every new header: ESP-IDF version guard, `esp_log` with module-specific tag, `esp_err_t` returns, snake_case naming, `esp_component.h` once-only include guard, semantic-version macro when applicable.

### What is explicitly OUT of issue #9 scope (not deliverable in this change)

- Full AP+STA bring-up, WiFi credential persistence, reconnect/backoff, NVS typed accessors → issue #21 (`wifi_manager`) and Fase 1 T1.2.x.
- Full ESP-NOW peer registration, recv-callback runtime, ACK/retry flow, node whitelist → Fase 1 T1.4.x.
- Full REST endpoints (`/api/status`, `/api/nodes`, …), WebSocket frame format, SPIFFS asset serving, Preact SPA → Fase 4 implementation.
- Full MQTT publish/subscribe lifecycle, broker connection state machine → Fase 1 T1.5.x.
- Full `esp_ota_*` flow, partition selection, rollback, signed-image verification → Fase 5 T5.x.
- Full ST7789 register writes, GPIO assignment for RES/DC/CS/SCL/SDA/BLK, backlight driver current-limit, DMA configuration → blocked on hardware validation (see open constraints).
- `firmware/gateway/web/` Preact SPA source → Fase 4.
- `firmware/common/protocol/` shared component → related dependency / future foundation item, not in issue #9 scope unless the user decides otherwise.
- `firmware/node/` ESP32-C3 firmware → not in issue #9 scope.
- `firmware/gateway/components/actuator_ctrl/` — listed in `CLAUDE.md` § Project Structure but not in issue #9's explicit component list. Treat as **out of scope for this change**. If later needed, it gets its own issue/change.
- Per-task T1.1.4 SNTP, T1.6.x watchdog tuning, T3.x sensor drivers → not in issue #9 scope.
- Any modification to `firmware/gateway/CMakeLists.txt`, `main.c` existing structure, `sdkconfig.defaults`, `partitions.csv`, `board_profile`, `board_rgb`, the existing test project files, or `CLAUDE.md`/`project.md`/`docs/hardware/*.md` — those are the merged deliverable from issue #8.

### Suggested change shape (foundation-only, per component)

Each wave is one component skeleton plus its test stub. Each skeleton compiles, declares the API surface, registers the component in `firmware/gateway/test/CMakeLists.txt` `TEST_COMPONENTS`, and has a "Contract" test case that asserts the declared function symbols exist and the documented enums/macros are reachable. **No on-target runtime evidence is required for a contract-only deliverable**; the strict TDD posture applies when each component's full implementation lands in its owning change (e.g., issue #21 for `wifi_manager`).

- **Wave A — NVS init composition**: add `nvs_config_init()` call (or inlined equivalent) to `main.c` between `board_profile_read()` and any future component init; document the NVS namespaces in the header (`"wifi"`, `"mqtt"`, `"ota"`, `"display"`, `"node"`) and default keys (`mqtt_namespace=iiot-kit`); Unity smoke: namespace table constants are reachable. **Smallest, isolated, sets the ordering convention.**
- **Wave B — `wifi_manager` interface only**: header with `wifi_manager_init()` signature, mode enum, default SSID constant; skeleton source returning `ESP_OK` with the documented log tag. **Issue #21 owns the body.** Unity test asserts the symbol exists. No overlap with #21 because this change does not implement event handlers, Kconfig.projbuild, or `wifi_credentials.local.h` (those are #21's surfaces).
- **Wave C — `espnow_manager` interface only**: header with `espnow_manager_init()` signature, recv-callback signature, peer-table-size constant (`20`), channel helper signature. Skeleton returning `ESP_OK`. Unity test asserts the symbol exists.
- **Wave D — `http_server` interface only**: header with `http_server_start()`/`http_server_stop()` signatures, default port constant. Skeleton returning `ESP_OK`. Unity test asserts the symbol exists.
- **Wave E — `mqtt_bridge` interface only (optional, Kconfig default off)**: header with `mqtt_bridge_init()` signature, namespace getter, broker URL struct. Skeleton returning `ESP_OK`. Unity test asserts the symbol exists.
- **Wave F — `ota_manager` interface only**: header with `ota_manager_init()` signature, partition-label enum, version getter. Skeleton returning `ESP_OK`. Unity test asserts the symbol exists.
- **Wave G — `display_manager` interface only (hardware hold)**: header with `display_manager_init()` signature, framebuffer-size constant, SPI-mode enum, **no GPIO assignment**, **no backlight driver assumption**, **no ST7789 register writes**. Skeleton returns `ESP_OK` and logs a documented placeholder. Unity test asserts the symbol exists and that the framebuffer constant equals `170 × 320 × 2`.

Each wave is sized to stay within the 400-line review budget. Wave A is the natural first commit (1–2 files). Waves B–G can be ordered per user decision; group size 2–3 keeps each PR near the budget.

### Open hardware constraints that bind every component

These must appear verbatim in any component header or spec produced by the future proposal phase:

- GPIO35, GPIO36, and GPIO37 are consumed by N16R8 Octal PSRAM — **unavailable for external use**.
- GPIO48 is reserved for the onboard WS2812 RGB LED.
- GPIO19 and GPIO20 are reserved for native USB while USB is needed.
- The withdrawn `0.1155 × 0.1155 mm` display pixel pitch and any geometry derived from it must not be used.
- The 8-pin display's GPIO mapping (RES/DC/CS/SCL/SDA/BLK), backlight current/control, and logic-level limits remain unresolved — **no display-signal GPIO assignments in this change**.
- `display_manager` skeleton must not invent logic levels, backlight driver topology, or ST7789 command sequences.

### Conflicts and dependencies

1. **Overlap with issue #21 (wifi_manager)**. Issue #21 has its own ODD tracker (`odd/tasks/issue-21-s3-apsta.md`) actively planning `wifi_manager` with WUC1–WUC4 work-unit commits, Kconfig.projbuild, gitignored `wifi_credentials.local.h`, and strict TDD. **Default recommendation**: issue #9 ships the `wifi_manager` **interface only** (header + symbol-only skeleton + Unity contract test); issue #21 owns the body. The two do not collide because issue #9 does not write `wifi_manager.c` runtime, event handlers, or Kconfig — only the public API. Confirm this split with the user in the proposal phase.
2. **Stale active change `openspec/changes/iiot-kit-transformation/`**. Per session preflight and the system addendum, this explore must not modify either the active or archived copies. Reference only.
3. **Generated artifacts stay local**. `firmware/gateway/sdkconfig`, `firmware/gateway/test/sdkconfig`, `firmware/gateway/build/`, `firmware/gateway/test/build/` are not staged; the new components must keep this convention and not add commits that stage generated files.
4. **Worktree state**. Session preflight flags the working tree as dirty/untracked on branch `chore/sdd-archive-iiot-kit-transformation`. The explore must not clean, reset, stage, commit, or modify any unrelated change; all exploration artifacts live under `openspec/changes/issue-9-gateway-firmware-foundation/`.
5. **`firmware/gateway/web/` (Preact SPA)** is out of phase ordering. Per `openspec/config.yaml` rules.proposal: *"Keep proposals compatible with the active educational phase ordering (Fases 0–6)."* Preact SPA delivery belongs to Fase 4 implementation, not Phase 0. Issue #9 must not scaffold the SPA.
6. **`firmware/node/` and `firmware/common/` are missing on disk** per `openspec/config.yaml`. They are not in issue #9 scope; the shared protocol component is a **related dependency / future foundation item** and must not be promoted into scope without an explicit user decision.

### Reconcile outcome

The issue #9 body is **partially out of date**: the project/board scaffold is already delivered by issue #8, but the **six component contracts** and **NVS init composition** explicitly listed in the issue body remain genuinely unimplemented. The fix is to scope this change to **contract-only skeletons + NVS init + minimal Unity contract tests**, not to attempt full implementations. Anything beyond a header + symbol-only skeleton + namespace/macro definitions belongs to its owning change (issue #21 for `wifi_manager`, Fase 1 for ESP-NOW/HTTP/MQTT/runtime NVS, Fase 4 for SPA, Fase 5 for OTA, future change for `actuator_ctrl`).

## Files Impact (relevant to the change scope)

| Category | Path | Reason |
|---|---|---|
| Existing scaffold (do not edit) | `firmware/gateway/CMakeLists.txt`, `firmware/gateway/main/main.c`, `firmware/gateway/main/CMakeLists.txt`, `firmware/gateway/sdkconfig.defaults`, `firmware/gateway/partitions.csv` | ODD-2/3 deliverable; merged |
| Existing components (do not edit) | `firmware/gateway/components/board_profile/`, `firmware/gateway/components/board_rgb/` | ODD-2/4 deliverable; merged |
| Existing tests (do not edit) | `firmware/gateway/test/CMakeLists.txt`, `firmware/gateway/test/main/test_main.c`, `firmware/gateway/components/board_profile/test/`, `firmware/gateway/components/board_rgb/test/` | ODD-2/4 deliverable; merged |
| Existing docs (do not edit) | `docs/hardware/esp32-s3-wroom-1-n16r8.md`, `docs/hardware/esp32-c6-lcd-1.47.md` (superseded), `CLAUDE.md`, `project.md`, `Fases/fase-01-gateway-nucleo.md`, `docs/replanificacion/02-protocolo-unificado.md` | Anchors for the change |
| Out of scope per issue body | `firmware/gateway/web/` (Preact SPA), `server/`, `dashboard/`, `firmware/node/`, `firmware/common/protocol/` | Fase 4 / future / pre-empted by iiot-kit-transformation |
| Optional / out of scope | `firmware/gateway/components/actuator_ctrl/` | Not in issue #9's explicit component list; would need its own issue |
| Gap (issue #9 contract-only deliverable) | `firmware/gateway/components/wifi_manager/`, `components/espnow_manager/`, `components/http_server/`, `components/mqtt_bridge/`, `components/ota_manager/`, `components/display_manager/` | Direct issue #9 gap, each as interface/skeleton only |
| Gap (composition) | `firmware/gateway/main/main.c` (NVS init call only), `firmware/gateway/test/CMakeLists.txt` (`TEST_COMPONENTS` registration of the six new components) | Part of Wave A |
| Concurrent work (not this change) | `odd/tasks/issue-21-s3-apsta.md` | Owns `wifi_manager` body; issue #9 ships interface only |
| Read-only references | `openspec/config.yaml`, `openspec/changes/iiot-kit-transformation/*` (active stale), `openspec/changes/archive/2026-09-27-iiot-kit-transformation/*` | Must not edit per session preflight |

## Open Questions (real decisions only)

1. **`wifi_manager` split with #21**: confirm the default — issue #9 ships the `wifi_manager` **interface only** (header + skeleton + Unity contract test); issue #21 owns the body (Kconfig, event handlers, credentials, strict TDD). The two should not write the same `.c` body.
2. **`mqtt_bridge` gating**: confirm the default — Kconfig default **off** so the gateway builds without an MQTT dependency, with the skeleton returning `ESP_OK` and never calling `esp_mqtt_client_init`.
3. **`display_manager` scope guard**: confirm the default — interface only, **no GPIO assignment**, **no backlight driver assumption**, **no ST7789 register writes**. The skeleton logs a documented placeholder and returns `ESP_OK`.
4. **`firmware/common/protocol/`**: confirm the default — **deferred**, treated as a related dependency or future foundation item, not part of this change.
5. **`actuator_ctrl`**: confirm the default — **out of scope** for this change. Listed in `CLAUDE.md` § Project Structure but absent from issue #9's explicit component list. Needs its own issue if/when it becomes required.
6. **`main.c` modification scope**: confirm the default — issue #9 may add only the **NVS init call** to `app_main` ordering (between `board_profile_read()` and any future component init), not restructure the existing main loop or any other behavior. Anything beyond that belongs to a later change.

## Recommended Approach

Issue #9 is best handled as a **scope-reconciliation, contract-only** change rather than fresh implementation:

1. **Acknowledge what issue #8 already delivered** in the proposal/spec so the change does not duplicate merged work.
2. **Carve the explicit gap** to the six component contracts, NVS init composition, and minimal Unity test skeletons. Mirror the existing `board_profile`/`board_rgb` layout for each: `CMakeLists.txt`, `include/<name>.h`, `<name>.c` (skeleton only), `test/test_<name>.c` (contract only).
3. **Defer `wifi_manager` body to issue #21**. Issue #9 ships only the non-overlapping interface seam so #21 can fill it without collisions.
4. **Defer shared protocol, SPA, node, full OTA/MQTT/display behavior** — these are not in the issue body.
5. **Mark `actuator_ctrl` out of scope** unless the user explicitly asks for it.
6. **Preserve all hard hardware constraints** verbatim in every new component header: GPIO35/36/37 unavailable, GPIO48 reserved for RGB LED, GPIO19/20 reserved for native USB, no unverified display GPIO assignments, no use of the withdrawn 0.1155 mm pixel pitch.
7. **Keep generated outputs untracked**: `sdkconfig`, `build/`; new components must not stage them.
8. **Phase delivery**: split per-component into sequenced waves (A: NVS init, B: `wifi_manager` interface, C: `espnow_manager`, D: `http_server`, E: `mqtt_bridge`, F: `ota_manager`, G: `display_manager`). Each wave stays small enough for the 400-line review budget.
9. **Ask the user** the six open questions above before any apply.