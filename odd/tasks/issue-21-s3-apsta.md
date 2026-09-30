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
- The Fase doc's T0.6.1 example still hardcodes `"iiot-kit2024"`. That is an example value, not policy; the implementation must read from the gitignored header, not copy the literal.
- RDD review switch: `receipt-driven development: on` at global scope. Native review candidates are work-unit commits, not TODOs or full branches. Push/PR remain user decisions.
- `firmware/gateway/sdkconfig` and `firmware/gateway/test/sdkconfig` are local-only (gitignored). `firmware/gateway/build/` and `firmware/gateway/test/build/` are already ignored.

## Secret handling policy (new)

- **`.env`** lives at the repo root and is gitignored (`/.env`). It carries `WIFI_MGR_LOCAL_STA_SSID=...` and `WIFI_MGR_LOCAL_STA_PASSWORD=...`. The user edits it by hand. It is the only canonical credential source during development.
- **`firmware/gateway/main/wifi_credentials.local.h`** is generated from `.env` by a small helper (`tools/gen-wifi-credentials.sh`). It is gitignored, never committed, and only present when the helper runs.
- **Falling back**: if `.env` is missing or empty, `wifi_manager_init()` reads `WIFI_MANAGER_AP_DEFAULT_PASSWORD` from Kconfig (default empty → AP open) and skips STA association. This keeps `idf.py build` reproducible in CI without secrets.
- **Build hook**: the helper script is invoked by `idf.py build` via `EXTRA_PRE_BUILD_COMMANDS` only when `WIFI_MGR_LOCAL_STA_SSID` is defined; otherwise it is a no-op. CI/local without `.env` builds cleanly.

## Work plan

### [x] ODD-21-WUC1 — `test(gateway): add wifi_manager scaffold and RED Unity config-helper tests`
**Status:** completed in commit `67708b3` on `feat/issue-21-s3-apsta` (off `main` `e75c2c7`).
**Surfaces:**
- `firmware/gateway/components/wifi_manager/{CMakeLists,Kconfig.projbuild,wifi_manager.c}`
- `firmware/gateway/components/wifi_manager/include/wifi_manager.h`
- `firmware/gateway/components/wifi_manager/test/{CMakeLists.txt,test_wifi_manager.c}`
- `firmware/gateway/main/wifi_credentials.local.h.example` (gitignored sibling for local secrets)
- `firmware/gateway/test/CMakeLists.txt` (registers `wifi_manager` via string concat in `TEST_COMPONENTS` — **legacy style**; replaced by defensive `list(FIND ...)` in WUC1').
- `.gitignore` lists `firmware/gateway/main/wifi_credentials.local.h`.

**Out of date:** the `test/CMakeLists.txt` registration uses the legacy string-concat style. After issue #9/#10 deliveries migrated every other component to the defensive `list(FIND ...)` pattern, WUC1 must be rebased onto `main` and rewritten to match.

### [ ] ODD-21-WUC1' — `test(gateway): rebase wifi_manager scaffold onto current main with defensive test registration and .env support`
**Status:** pending.
**Route:** parent inline implementation + interactive rebase of `67708b3`.
**Planned surfaces:**
- `firmware/gateway/test/CMakeLists.txt` — replace legacy string registration of `wifi_manager` with the defensive `list(FIND ...)` block matching the other 11 components.
- `firmware/gateway/main/wifi_credentials.local.h.example` — add `#include "../.env-loader.h"` reference and a small README note pointing to the helper script.
- `tools/gen-wifi-credentials.sh` (new) — bash helper that reads `/Dev/Codex/ESP32-IIoT-Kit/.env` and emits `firmware/gateway/main/wifi_credentials.local.h` with the Wi-Fi macros; no-op if `.env` is missing.
- `.gitignore` — append `/.env`, `/tools/gen-wifi-credentials.sh.lock`, and the generated `wifi_credentials.local.h` (already present).
- `.env.example` (new, committed) — empty placeholder documenting the keys and the secret-handling policy.

**Checks/evidence:** `git diff e75c2c7..feat/issue-21-s3-apsta -- firmware/gateway/test/CMakeLists.txt` shows the defensive block. `bash tools/gen-wifi-credentials.sh --dry-run` reads `.env` and prints the generated file path. Build still RED for the helpers (no implementation yet).

**TDD discipline:** No implementation change in WUC1'. Only test registration refactor + secret-handling scaffolding.

### [ ] ODD-21-WUC2 — `feat(gateway): implement wifi_manager AP+STA init, event handlers, and credential loader (GREEN)`
**Status:** pending.
**Route:** parent inline implementation, TDD strict mode.
**Planned surfaces:**
- `firmware/gateway/components/wifi_manager/wifi_manager.c` — full implementation:
  - `wifi_manager_init` orchestrates: netif init → default event loop → Wi-Fi driver init with default config → mode set to `WIFI_MODE_APSTA` → AP config built via helpers → STA config built via helpers → `esp_wifi_set_config(WIFI_IF_AP, …)` + `esp_wifi_set_config(WIFI_IF_STA, …)` → `esp_wifi_start()` → register event handlers for the six events listed in scope → kick STA association via `esp_wifi_connect()`.
  - Event handlers log: AP client MAC on `WIFI_EVENT_AP_STACONNECTED`, AP client MAC on `WIFI_EVENT_AP_STADISCONNECTED`, channel on `WIFI_EVENT_STA_START`, `SYSTEM_EVENT_STA_CONNECTED` reason code on `WIFI_EVENT_STA_CONNECTED`, disconnect reason on `WIFI_EVENT_STA_DISCONNECTED` (auto-reconnect via `esp_wifi_connect()` after a 1 s delay using `esp_timer`), assigned IP on `IP_EVENT_STA_GOT_IP` (and `IP_EVENT_STA_LOST_IP` for completeness).
  - Credential loader: `#ifdef WIFI_MGR_LOCAL_STA_SSID` reads `wifi_credentials.local.h`; otherwise falls back to `WIFI_MANAGER_AP_DEFAULT_PASSWORD` from Kconfig and STA stays disabled.
  - `wifi_manager_deinit` does the symmetric teardown.
  - All public helpers (validate_ap_channel, ssid_in_range, password_in_range, format_mac, apply_credentials) are implemented and pass the RED tests.

**Checks/evidence:**
- `idf.py -C firmware/gateway/test build` succeeds.
- `strings build/gateway_test.elf | grep -oE '\[(...)\]'` shows `[wifi_manager]`.
- `xtensa-esp32s3-elf-nm build/gateway_test.elf | grep -E 'wifi_manager_'` lists `wifi_manager_init / deinit / validate_ap_channel / ssid_in_range / password_in_range / format_mac / apply_credentials`.
- Manual launch: `idf.py -C firmware/gateway/test -p /dev/ttyACM0 flash monitor` and capture `[wifi_manager]` logs plus 8 GREEN test cases.
- STA DHCP boot smoke only if `.env` carries real SSID + password AND user authorizes `/dev/ttyACM0` for the gateway app. Without credentials, STA is a no-op and AP comes up open.

**Constraints:** Never embed real passwords. `wifi_manager.c` reads from `wifi_credentials.local.h` only; that header is generated and gitignored.

### [ ] ODD-21-WUC3 — `feat(gateway): wire wifi_manager_init into gateway app_main`
**Status:** pending.
**Route:** parent inline implementation.
**Planned surfaces:** `firmware/gateway/main/main.c`, `firmware/gateway/main/CMakeLists.txt` (add `wifi_manager` to `REQUIRES`).
**Checks/evidence:** `idf.py -C firmware/gateway set-target esp32s3 && idf.py -C firmware/gateway build` passes. Boot smoke on `/dev/ttyACM0` only if user authorizes replacing the test app for this step.

### [ ] ODD-21-WUC4 — `docs(odd): close ODD-21 with work-unit commit evidence and build/flash results`
**Status:** pending.
**Route:** parent inline documentation.
**Planned surfaces:** this file, `CLAUDE.md` (only if a durable rule changed).
**Checks/evidence:** Record each work-unit commit hash, RED/GREEN serial output, and any deferred items. Add a permanent note about secret-handling policy under "Common mistakes" in `CLAUDE.md` so future slices do not regress it. Note the channel-sharing reminder for T0.6.3.

## Acceptance criteria

- `wifi_manager` component compiles cleanly, links into the gateway app, and passes its Unity tests.
- `IIoT-Gateway` AP appears and accepts a WPA2-PSK connection from a phone when `WIFI_MANAGER_AP_DEFAULT_PASSWORD` is non-empty (or `wifi_credentials.local.h` defines `WIFI_MGR_LOCAL_AP_PASSWORD`); the gateway logs `WIFI_EVENT_AP_STACONNECTED` with the client MAC.
- STA joins the user's test router when `.env` carries real SSID + password AND user authorizes the gateway-app flash; the gateway logs `IP_EVENT_STA_GOT_IP` with the assigned IP.
- No secret is present in source, commits, the tracker, or the GitHub issue. `.env` and `wifi_credentials.local.h` are gitignored.
- Display remains disconnected; no ESP-NOW/APSTA coexistence is claimed.

## Progress and verification

- Issue #21 (`status:approved`) authorizes AP+STA on the S3 candidate. Display-independent. Credentials explicitly off-source.
- Branch `feat/issue-21-s3-apsta` at `67708b3` (WUC1 done). Behind `origin/main` by 29 commits — needs interactive rebase.
- WUC1 commit `67708b3` ships RED Unity tests for `validate_ap_channel`, `ssid_in_range`, `password_in_range`, `format_mac`, `apply_credentials`. 5 of 8 cases fail on-target with `Expected 0 Was 268` (NOT_FINISHED).
- `gentle-ai-worker` skill remains absent on this machine; the user authorized inline implementation for this candidate. Review switch stays on; native review runs at the work-unit commit boundary if the user opens one.

## Next step

Begin WUC1': rebase `feat/issue-21-s3-apsta` onto current `main`, migrate `test/CMakeLists.txt` to the defensive `list(FIND ...)` block, add `tools/gen-wifi-credentials.sh`, `.gitignore` (`.env`), and `.env.example`. Then WUC2 (GREEN), WUC3 (wire), WUC4 (close).
