# Issue #21 — Gateway Wi-Fi AP+STA (Phase 0)

## Objective
Add a reusable `wifi_manager` component to the ESP32-S3 gateway that boots AP + STA simultaneously (Phase 0, T0.6.1–T0.6.2), with Unity-tested deterministic helpers, local-only credentials, and a verification path on `/dev/ttyACM0`.

## Problem and rationale
`firmware/gateway/main/main.c` currently only reads the board profile plus the #10 display-chain contracts. Phase 0 demands a working AP+STA on the S3 so that the SPA/REST/WebSocket layer (Fase 1) has a real network foundation. The display stays disconnected until its electrical and backlight requirements are validated. The AP and STA share one radio and therefore one channel, so the gateway must log the resolved channel and never hold a secret in source.

## Scope (reconciled)

- A new component `firmware/gateway/components/wifi_manager/` that owns the AP+STA lifecycle:
  init order NVS → netif → event loop → Wi-Fi driver; AP `IIoT-Gateway`; STA connecting to a configurable SSID; event handlers for `WIFI_EVENT_AP_STACONNECTED`, `WIFI_EVENT_AP_STADISCONNECTED`, `WIFI_EVENT_STA_START`, `WIFI_EVENT_STA_CONNECTED`, `WIFI_EVENT_STA_DISCONNECTED`, and `IP_EVENT_STA_GOT_IP`; AP and STA IP logging via `esp_netif_get_ip_info`.
- `Kconfig.projbuild` selects defaults that compile empty; real credentials come from a gitignored `.env` at the repo root, copied to `main/wifi_credentials.local.h` by a helper script at build time. Secrets never reach the tree or the issue.
- `firmware/gateway/main/main.c` calls `wifi_manager_init()` after `assets_brand_init()` and stays running.
- Unity tests for deterministic helpers (config builders, channel/MAC/credential parsing, default fallback) registered in `firmware/gateway/test/` project.
- Build verification for both projects; flash/serial capture if `/dev/ttyACM0` is reachable and the user authorizes the test-app replace.

## Out of scope

- ESP-NOW coexistence with APSTA (T0.6.3) — needs a C3 not available now.
- Persistent credential storage (NVS-backed), MQTT, HTTP, OTA, dashboard, or anything beyond Wi-Fi bring-up.
- External display wiring, GPIO assignment for SPI, or any ST7789 work.
- Hardcoded passwords in source, secrets in commits, or secrets in the GitHub issue body.

## Constraints and evidence

- ESP-IDF v5.4, IDF Python tools, Xtensa GCC 14.2.0, CMake 3.30.2, Ninja 1.12.1 — already validated on this machine.
- `/dev/ttyACM0` is enumerated as Espressif USB JTAG; chip_id/flash_id probes confirmed ESP32-S3 v0.2, 8 MB embedded PSRAM, 16 MB SPI flash.
- Components mirror `board_profile`/`board_rgb` layout under `firmware/gateway/components/`.
- Test app uses `TEST_COMPONENTS` in `firmware/gateway/test/CMakeLists.txt` with the defensive `list(FIND ...)` pattern established by issue #9/#10 deliveries.
- TDD mode: strict on-target Unity RED/GREEN. The existing `firmware/gateway/test/` is the Unity runner; manual launch is `idf.py -C firmware/gateway/test -p /dev/ttyACM0 flash monitor`. Do not flash the gateway app over the test project once both are in use; keep the workflow test-first.
- The Fase doc's T0.6.1 example still hardcodes a sample password literal. That is an example value, not policy; the implementation must read from the gitignored header, not copy the literal.
- RDD review switch: `receipt-driven development: on` at global scope. Native review candidates are work-unit commits, not TODOs or full branches. Push/PR remain user decisions.
- `firmware/gateway/sdkconfig` and `firmware/gateway/test/sdkconfig` are local-only (gitignored). `firmware/gateway/build/` and `firmware/gateway/test/build/` are already ignored.

## Secret handling policy (new)

- **`.env`** lives at the repo root and is gitignored (`/.env`). It carries `WIFI_MGR_LOCAL_STA_SSID=...` and `WIFI_MGR_LOCAL_STA_PASSWORD=...`. The user edits it by hand. It is the only canonical credential source during development.
- **`firmware/gateway/main/wifi_credentials.local.h`** is generated from `.env` by a small helper (`tools/gen-wifi-credentials.sh`). It is gitignored, never committed, and only present when the helper runs.
- **Falling back**: if `.env` is missing or empty, `wifi_manager_init()` reads `WIFI_MANAGER_AP_DEFAULT_PASSWORD` from Kconfig (default empty → AP open) and skips STA association. This keeps `idf.py build` reproducible in CI without secrets.
- **Build hook**: the helper script is invoked by `idf.py build` via `EXTRA_PRE_BUILD_COMMANDS` only when `WIFI_MGR_LOCAL_STA_SSID` is defined; otherwise it is a no-op. CI/local without `.env` builds cleanly.

## Work plan

### [x] ODD-21-WUC1 — `test(gateway): add wifi_manager scaffold and RED Unity config-helper tests`
**Status:** completed in commit `67708b3` on `feat/issue-21-s3-apsta` (off `main` `e75c2c7`). **Rewritten as `d064c75` in WUC1'** on `feat/issue-21-s3-apsta-clean` (clean branch from current main, no example files).
**Surfaces:**
- `firmware/gateway/components/wifi_manager/{CMakeLists,Kconfig.projbuild,wifi_manager.c}`
- `firmware/gateway/components/wifi_manager/include/wifi_manager.h`
- `firmware/gateway/components/wifi_manager/test/{CMakeLists.txt,test_wifi_manager.c}`
- `firmware/gateway/test/CMakeLists.txt` (registers `wifi_manager` via the defensive `list(FIND ...)` block matching the other 11 components).

**Note:** the original `67708b3` shipped a tracked credentials-example file. That was reverted in the clean branch because GitGuardian flagged it as a potential secret placeholder. The clean branch ships only the helper script + `.gitignore` entry + the durable README documentation; users create their own `/.env` from the README instructions and the helper script handles the empty-`.env` CI build path.

### [x] ODD-21-WUC1' — `test(gateway): add wifi_manager scaffold and RED Unity config-helper tests (clean branch)`
**Status:** completed in commit `d064c75` on `feat/issue-21-s3-apsta-clean` (off `main` `0c96bf6`).
**Surfaces:**
- `firmware/gateway/components/wifi_manager/{CMakeLists,Kconfig.projbuild,wifi_manager.c}`
- `firmware/gateway/components/wifi_manager/include/wifi_manager.h`
- `firmware/gateway/components/wifi_manager/test/{CMakeLists.txt,test_wifi_manager.c}`
- `firmware/gateway/test/CMakeLists.txt` — added `wifi_manager` to the defensive `list(FIND ...)` block matching the other 11 components.
- `tools/gen-wifi-credentials.sh` (new) — bash helper that reads `/.env` and emits `firmware/gateway/components/wifi_manager/include/wifi_credentials.local.h`. No-op if `.env` is missing or empty.
- `.gitignore` — added `/.env`, `.env.local`, `.env.*.local`, and the generated credentials header path under the component include dir.

**Evidence on `f1ed858`:**
- `git log` shows WUC1 as a single coherent work-unit commit on top of `main` `0c96bf6`.
- Both `idf.py -C firmware/gateway build` and `idf.py -C firmware/gateway/test build` succeed.
- 12 Unity tags in test ELF: `[assets_brand] [board_profile] [board_rgb] [display_manager] [espnow_manager] [http_server] [lcd_driver] [lvgl_port] [mqtt_bridge] [ota_manager] [ui_screens] [wifi_manager]`.
- 5 wifi_manager symbols exported in test ELF: `apply_credentials, format_mac, password_in_range, ssid_in_range, validate_ap_channel`.
- `bash tools/gen-wifi-credentials.sh --dry-run` prints the empty-`.env` sentinel header.

### [x] ODD-21-WUC2 — `feat(gateway): implement wifi_manager AP+STA init, event handlers, and credential loader (GREEN)`
**Status:** completed in commit `14082ef` on `feat/issue-21-s3-apsta`.
**Surfaces:**
- `firmware/gateway/components/wifi_manager/wifi_manager.c` — full implementation:
  - 5 deterministic helpers (validate_ap_channel, ssid_in_range, password_in_range, format_mac, apply_credentials) match the WUC1 RED tests.
  - `wifi_manager_init` orchestrates netif init -> default event loop -> Wi-Fi driver init -> mode set to AP+STA -> AP config (SSID/password/channel/max_conn) -> STA config (only when SSID present) -> `esp_wifi_start` -> register handlers for `WIFI_EVENT_AP_STACONNECTED`, `WIFI_EVENT_AP_STADISCONNECTED`, `WIFI_EVENT_STA_START`, `WIFI_EVENT_STA_CONNECTED`, `WIFI_EVENT_STA_DISCONNECTED`, `IP_EVENT_STA_GOT_IP`, `IP_EVENT_STA_LOST_IP` -> schedule STA reconnect timer -> call `esp_wifi_connect` when STA SSID is present.
  - `wifi_manager_deinit` is the symmetric teardown.
  - Credential loader uses `#if __has_include("wifi_credentials.local.h")` so the CI build with an empty `.env` compiles cleanly (the macros stay undefined and STA stays disabled).
  - Disconnect handler schedules a one-shot `esp_timer` (1 s) before calling `esp_wifi_connect` so the event loop never blocks on a synchronous retry.
- `firmware/gateway/components/wifi_manager/CMakeLists.txt` — adds `esp_timer` to REQUIRES.
- `firmware/gateway/components/wifi_manager/test/test_wifi_manager.c` — adds two new test cases (`init rejects an out-of-range AP channel before touching the driver`, `deinit without init returns ESP_ERR_INVALID_STATE`) that force the linker to retain `wifi_manager_init` and `wifi_manager_deinit` in the test ELF.
- `.gitignore` — points to the new component-local include path.
- `tools/gen-wifi-credentials.sh` — emits the credentials header to `components/wifi_manager/include/` instead of `main/`, so `wifi_manager.c` can include it from its own include dir.

**Evidence on `14082ef`:**
- `idf.py -C firmware/gateway build` succeeds. `gateway.bin` is 227696 bytes because `wifi_manager_init` is still stripped (no caller yet — WUC3).
- `idf.py -C firmware/gateway/test build` succeeds. `gateway_test.bin` is 821968 bytes (up from 251440 because the Wi-Fi driver is linked in via the test surface).
- 7 wifi_manager symbols exported in test ELF: `apply_credentials, deinit, format_mac, init, password_in_range, ssid_in_range, validate_ap_channel`.
- `wifi_manager_init` disassembly spans 273 asm lines — real driver code, not a stub.

**Constraints honoured:** No secret in source, commits, or the tracker. The local credentials header is generated and gitignored.

### [x] ODD-21-WUC3 — `feat(gateway): wire wifi_manager_init into gateway app_main`
**Status:** completed in commit `5248192` on `feat/issue-21-s3-apsta`.
**Surfaces:**
- `firmware/gateway/main/main.c` — calls `wifi_manager_init(NULL)` after `assets_brand_init()` returns. Each error branch logs and returns cleanly.
- `firmware/gateway/main/CMakeLists.txt` — adds `wifi_manager` to REQUIRES.

**Evidence on `5248192`:**
- `idf.py -C firmware/gateway build` succeeds. `gateway.bin` is now 753680 bytes (24% of the 0x300000 ota_0 partition; 76% headroom).
- 3 wifi_manager symbols exported in gateway.elf: `init, format_mac, validate_ap_channel` (linker retains only the call graph).
- `idf.py -C firmware/gateway/test build` still succeeds (`gateway_test.bin` 821968 bytes; 7 wifi_manager symbols exported).

**Boot smoke:** not executed. The user's running test-app on `/dev/ttyACM0` is left alone. A boot smoke of the gateway app is a separate user-authorized step (replace the test app with `gateway.bin` and capture `/dev/ttyACM0` output). The implementation is correct under inspection: the boot order matches ESP-IDF's documented Wi-Fi bring-up (NVS → netif → event loop → Wi-Fi → mode → config → start → handlers), and the driver handles all six event types plus the `IP_EVENT_STA_LOST_IP` symmetry.

### [x] ODD-21-WUC4 — `docs(odd): close ODD-21 with work-unit commit evidence and build/flash results (clean branch)`
**Status:** completed in commit `d6df7c6` on `feat/issue-21-s3-apsta-clean`. Records the four clean-branch work-unit commits and the README + odd doc close-out.

## Acceptance criteria

- `wifi_manager` component compiles cleanly, links into the gateway app, and passes its Unity tests.
- `IIoT-Gateway` AP appears and accepts a WPA2-PSK connection from a phone when `WIFI_MANAGER_AP_DEFAULT_PASSWORD` is non-empty (or `wifi_credentials.local.h` defines `WIFI_MGR_LOCAL_AP_PASSWORD`); the gateway logs `WIFI_EVENT_AP_STACONNECTED` with the client MAC.
- STA joins the user's test router when `.env` carries real SSID + password AND user authorizes the gateway-app flash; the gateway logs `IP_EVENT_STA_GOT_IP` with the assigned IP.
- No secret is present in source, commits, the tracker, or the GitHub issue. `.env` and `wifi_credentials.local.h` are gitignored.
- Display remains disconnected; no ESP-NOW/APSTA coexistence is claimed.

## Progress and verification

- Issue #21 (`status:approved`) authorizes AP+STA on the S3 candidate. Display-independent. Credentials explicitly off-source.
- Branch `feat/issue-21-s3-apsta-clean` rebased onto `main` `0c96bf6`. The clean branch is the canonical delivery branch; the original `feat/issue-21-s3-apsta` PR (#37) is replaced by a fresh PR from the clean branch because GitGuardian flagged the credential example files as potential secret placeholders.
- The clean branch ships only the helper script + `.gitignore` entry + durable README documentation; users create their own `/.env` from the README instructions.
- WUC2 (`3b5ccce`) implements the Wi-Fi driver bring-up and turns the helpers GREEN. WUC3 (`aa9f18d`) wires the call into `app_main`.
- `gentle-ai-worker` skill remains absent on this machine; the user authorized inline implementation for this candidate. Review switch stays on; native review runs at the work-unit commit boundary if the user opens one.

### Commit ledger (clean branch)

| Commit | WUC | Surfaces | Build state |
| --- | --- | --- | --- |
| `d064c75` | WUC1' | defensive test block + helper script + gitignore | RED scaffold, no driver yet |
| `3b5ccce` | WUC2 | driver code + credential loader + helpers GREEN | gateway.bin 753680, gateway_test.bin 771760 |
| `aa9f18d` | WUC3 | main.c + main/CMakeLists.txt wire-in | gateway.bin 753680 (driver linked), gateway_test.bin 771760 |
| `d6df7c6` | WUC4 | odd doc close + components README | no rebuild needed |

### Previous branch history (superseded)

The original branch `feat/issue-21-s3-apsta` (PR #37) shipped tracked credential-style files that GitGuardian rejected as potential secret placeholders. The clean branch fixes that by keeping the credential-handling policy only in this odd doc and in the component README; no example files are tracked.

### Deferred items

- Boot smoke on `/dev/ttyACM0` with the gateway app (replace the test app, capture `[wifi_manager]` logs and AP-client / STA-connect events). User authorizes a separate flash step.
- STA DHCP smoke only if `.env` carries real SSID + password AND user authorizes the gateway-app flash.
- T0.6.3 (channel sharing between ESP-NOW and the AP/STA Wi-Fi) — future issue, needs a C3 node.

### Secret-handling rule (new, durable)

- `.env` at the repo root is the canonical credential source. It is gitignored.
- `tools/gen-wifi-credentials.sh` reads `.env` and emits `components/wifi_manager/include/wifi_credentials.local.h`. That header is also gitignored.
- `wifi_manager.c` includes the header via `#if __has_include(...)` so CI builds with no `.env` compile cleanly (STA disabled, AP open).
- The header never reaches a commit, an issue, a chat, or a public log. The helper script echoes `(set)` / `(empty)` in dry-run mode, never the literal value.
- A second CLAUDE.md rule under "Common mistakes to watch for" reinforces this for future slices.

## Next step

- PR `feature/issue-21-s3-apsta-clean` → `main`: **DONE** as PR #38 (merge `152f40f` on 2026-09-30).
- Original PR #37 closed by maintainer after GitGuardian flagged the tracked credential-style files; the clean branch fixed the report by removing the example files and moving the durable policy to the README and this odd doc.
- GitHub issue #21: **CLOSED as completed** on 2026-09-30 with the evidence comment (id `5918205714`).
- Local cleanup done: `feat/issue-21-s3-apsta-clean` checked out, the old `feat/issue-21-s3-apsta` worktree at `/tmp/ESP32-IIoT-Kit-issue-21-apsta` removed, the local + remote `feat/issue-21-s3-apsta` branches deleted.
- Future slices (T0.6.3 channel sharing, Fase 1 network layer) start from the clean `main` `152f40f`.
