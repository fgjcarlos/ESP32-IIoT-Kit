# Archive report: issue-9-gateway-firmware-foundation

**Change**: issue-9-gateway-firmware-foundation
**Date**: 2026-09-29
**Artifact store**: openspec
**Status**: completed
**Outcome**: closed (merged to `main` at `41449df`)

## Phase history

| Phase | Status | Date | Evidence |
|---|---|---|---|
| explore | completed | 2026-09-27 | `explore.md` (repository + context reconnaissance) |
| proposal | completed | 2026-09-27 | `proposal.md` (issue framing + scope) |
| spec | completed | 2026-09-27 | `specs/gateway-firmware-foundation/spec.md` (5 component contracts + TEST_COMPONENTS + NVS bootstrap) |
| design | completed | 2026-09-27 | `design.md` (component-by-component design + slicing plan) |
| tasks | completed | 2026-09-27 | `tasks.md` (9 boxes, all `[x]` at archive time) |
| apply | completed | 2026-09-29 | `apply-progress.md` (8 slices documented; 6 PRs opened; 6 PRs merged; PR #28 closed with conflict, replaced by PR #29) |
| verify | completed | 2026-09-29 | `verify-report.md` (T-01.07 build/test + T-01.08 boot smoke captured verbatim) |
| archive | completed | 2026-09-29 | this document |

## Slicing outcome

The chained one-component-per-PR delivery strategy was selected (per the spec scenario and `ask-on-risk` decision). Six merged PRs in dependency order:

1. **PR #23** — T-00.01 NVS bootstrap composition (commit `18fe7e1`).
2. **PR #24** — T-01.01 espnow_manager component contract (commit `0c7ba93`).
3. **PR #25** — T-01.02 http_server component contract (commit `f9e2726`).
4. **PR #26** — T-01.03 mqtt_bridge component contract (commit `9c6ebdc`).
5. **PR #27** — T-01.04 ota_manager component contract (commit `98db781`).
6. **PR #29** — T-01.05 display_manager component contract with hardware hold (commit `a8c877d`).

One PR closed without merging:
- **PR #28** — display_manager contract, original (commit `b2ac7e4`). Closed after PR #27 introduced a conflict in `firmware/gateway/test/CMakeLists.txt` (two adjacent `list(FIND ...)` appends). The Pi safety policy blocks `git push --force-with-lease`, so the rebased commit was reopened as PR #29 on a new branch `feature/issue-9-display-manager-rebased`. The original branch `origin/feature/issue-9-display-manager` is left in place at `b2ac7e4` for traceability; can be deleted with `git push origin --delete feature/issue-9-display-manager` after a follow-up cleanup PR.

The T-01.06 TEST_COMPONENTS append was distributed across the six component PRs (each PR added one defensive `list(FIND ...)` block). The cumulative TEST_COMPONENTS final composition is `board_profile;board_rgb;espnow_manager;http_server;mqtt_bridge;ota_manager;display_manager`.

T-01.07 (cumulative build/test evidence) was captured once on `main` after PR #29 merged, using a fresh worktree at `41449df`. T-01.08 (physical-board boot smoke) was captured immediately after on `/dev/ttyACM0`.

## Guard rails compliance

- `**/wifi_manager/**` — not touched (owned by issue #21 / `odd/tasks/issue-21-s3-apsta.md`).
- `firmware/common/**`, `firmware/node/**`, `firmware/gateway/web/**`, `firmware/gateway/components/actuator_ctrl/**` — not touched.
- `firmware/gateway/CMakeLists.txt`, `sdkconfig.defaults`, `partitions.csv` — not edited.
- `firmware/gateway/components/board_profile/**`, `board_rgb/**`, `test/main/**`, existing `board_*/test/**` — not touched.
- All documentation (`CLAUDE.md`, `project.md`, `Fases/*`, `docs/*`, `docs_site/*`, `Tutorial/*`, `knowledge/*`) — not touched.
- `openspec/changes/iiot-kit-transformation/**` and `openspec/changes/archive/2026-09-27-iiot-kit-transformation/**` — not touched.
- Generated outputs (`sdkconfig`, `sdkconfig.old`, `build/`, `*.bin`, `*.elf`, `.cache/`) — never staged; gitignored.
- Dirty worktree on `chore/sdd-archive-iiot-kit-transformation` — never touched; all work happened in dedicated worktrees.

## Component-level guard rails

- Five new components, zero `Kconfig` files introduced.
- `mqtt_bridge` does NOT include `mqtt_client.h`, does NOT list `mqtt` in `REQUIRES`, does NOT call any `esp_mqtt_client_*` API.
- `display_manager` does NOT assign any GPIO, does NOT call any SPI / ST7789 / backlight / framebuffer API, does NOT use the withdrawn `0.1155 × 0.1155 mm` pixel pitch or any geometry derived from it; the six hardware-hold bullets are reproduced verbatim from `specs/gateway-firmware-foundation/spec.md` and exposed in the public header.
- `ota_manager` does NOT call `esp_ota_*`, does NOT include `esp_ota.h`, does NOT write partitions or toggle bootable flags, does NOT reboot.
- All headers follow the `board_profile` / `board_rgb` conventions: `#pragma once`, `esp_err.h` include, `esp_err_t`-returning public functions, snake_case naming, source-local `TAG`, no ESP-IDF version guard.

## What is NOT in this archive

- `wifi_manager` work — owned by issue #21; deferred to its own archive entry.
- Per-component runtime implementations — owned by Fase 1–5 work; this change delivers contract-only skeletons.
- On-target `idf.py test` execution — deferred to a future CI or smoke change because the test harness policy is not on-path in this environment; the compile-time equivalent (Unity tag presence + symbol export) is the verifiable evidence.
- Functional verification of each component (WiFi bring-up, ESP-NOW peer add, REST request, WebSocket frame, SPIFFS asset serving, MQTT publish, OTA slot switch, ST7789 init, NVS typed accessor, sensor drivers, SNTP) — each lives in its own component's runtime cycle.

## Review workload (delivered)

| Slice | PR | Files | Lines | Status |
|---|---|---|---|---|
| Slice 1 (T-00.01) | #23 | 3 created + 2 edited | ~70 | MERGED |
| Slice 2 (T-01.01) | #24 | 5 created | 39 | MERGED |
| Slice 3 (T-01.02) | #25 | 5 created | 39 | MERGED |
| Slice 4 (T-01.03) | #26 | 5 created | 40 | MERGED |
| Slice 5 (T-01.04) | #27 | 9 changed | +79 | MERGED |
| Slice 6 (T-01.05) | #29 | 8 changed | +127 (rebased, was +161 in PR #28) | MERGED |
| Slice 7 (T-01.07) | n/a | 0 (evidence only) | 0 | closed |
| Slice 8 (T-01.08) | n/a | 0 (evidence only) | 0 | closed |
| Slice 9 (archive) | n/a | 0 (this document) | 0 | closed |

Aggregate ≈640 changed lines across ≈28 files, distributed across six review-sized PRs (each ≤ +127 lines, well inside the 400-line budget). `delivery_strategy: ask-on-risk` was honored before each push+PR cycle.

## References

- GitHub issue: https://github.com/fgjcarlos/ESP32-IIoT-Kit/issues/9
- `main` merge commit: `41449df` (Merge of PR #29)
- `odd/tasks/issue-9-gateway-firmware-foundation.md` (full ODD traceability)
- `odd/tasks/issue-21-s3-apsta.md` (sibling change, not in this archive)
- Engram memory `sdd/issue-9-gateway-firmware-foundation/progress` (cumulative session observations)
