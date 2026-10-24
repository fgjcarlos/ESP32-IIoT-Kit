# AGENTS.md

ESP32-IIoT-Kit — generic industrial monitoring/control platform on ESP32: an ESP32-S3 gateway (ESP-IDF v5.x, C11) plus ESP32-C3 sensor nodes over an ESP-NOW backbone. Educational project; phases are sequential.

## Setup commands

- Load toolchain first: `source $HOME/esp/esp-idf/export.sh` — `idf.py` is not on `PATH` by default
- Build firmware: `cd firmware/gateway && idf.py build`
- Run unit tests: `cd firmware/gateway/test && idf.py test` (Unity, on-target; per-component)
- Build docs: `mkdocs build --strict` (needs `pip install mkdocs-material mkdocs-roamlinks-plugin`)
- Set target (new project only): `idf.py set-target esp32s3`

No workspace-wide test/lint command exists. There is no formatter or linter configured at workspace level; `gcc -Wall -Wextra` via the ESP-IDF compiler is the only static check.

## Project layout

- `firmware/gateway/` — ESP-IDF project for the ESP32-S3 (target `esp32s3`, 16MB flash, 8MB Octal PSRAM)
- `firmware/gateway/components/` — one directory per reusable component
- `firmware/gateway/main/` — `app_main`, `nvs_config`
- `firmware/gateway/test/` — top-level Unity test project (`TEST_COMPONENTS` list drives what runs)
- `firmware/common/protocol/` — shared wire protocol (source of truth: `docs/replanificacion/02-protocolo-unificado.md`)
- `openspec/` — spec-driven change workflow: `changes/<id>/{proposal,design,tasks}.md` + `specs/<capability>/spec.md`; archive to `changes/archive/<date>-<id>/`
- `odd/tasks/` — per-issue task plans (`issue-NN-slug.md`) with scope, constraints, and work plan
- `docs_site/`, `Tutorial/`, `Fases/`, `knowledge/` — MkDocs site and Obsidian learning vault (European Spanish)
- `docs/replanificacion/` — authoritative decisions and corrected phase plan
- `examples/fish-farm/` — domain example on the same platform

`firmware/node/` (ESP32-C3 sensor nodes) and `firmware/gateway/web/` (Preact dashboard) are **planned but not on disk** — do not assume they are runnable.

## Code style

- C11, ESP-IDF v5.x conventions — never Arduino
- `snake_case` functions/variables, module-prefixed API (`wifi_manager_init`, `espnow_manager_send`)
- `ESP_LOGx` with a module-specific `TAG`; `printf` is forbidden
- `ESP_ERROR_CHECK()` for fatal paths, `esp_err_t` returns for recoverable ones — always check returns
- One NVS namespace per module (`"wifi"`, `"espnow"`, `"mqtt"`)
- Component layout: `include/<name>.h`, `<name>.c`, `CMakeLists.txt`, `test/CMakeLists.txt`, `test/test_<name>.c`; register new components in `firmware/gateway/test/CMakeLists.txt` `TEST_COMPONENTS` using the defensive `list(FIND ...)` pattern
- Doxygen `@brief`/`@return` on public headers; document hardware assumptions as `pending physical verification`
- Docs narrative in European Spanish; `openspec/` and `.atl/` artifacts in English

## Testing instructions

- Unit tests: `cd firmware/gateway/test && idf.py test` (Unity `TEST_CASE` blocks in `components/*/test/test_*.c`)
- Host-side unit tests are not configured; on-target execution is the only layer
- New behavior needs a Unity test in the owning component's `test/` directory — compile success plus Unity tag presence is the verifiable equivalent when hardware is unavailable
- Firmware must build clean before tests are claimed green

## Hardware and protocol constraints

- ESP-NOW and WiFi must share the same channel; the gateway fixes the channel
- ESP-NOW: max 250 bytes/frame, max 20 encrypted peers
- GPIO35/36/37 are occupied by Octal PSRAM; GPIO48 is the onboard RGB LED; GPIO19/20 are native USB — do not reassign
- GPIO22 as LCD backlight is a **candidate only**, pending physical carrier verification
- The seller pixel pitch `0.1155 mm` is **withdrawn** — never reuse it or geometry derived from it
- The LCD's reported 20 mA is unscoped (module vs. backlight vs. per-LED) — do not treat it as a backlight limit
- Display contact map (1 GND, 2 VCC, 3 SCL, 4 SDA, 5 RES, 6 DC, 7 CS, 8 BLK) is display-side, not GPIO assignment
- FreeRTOS default task stack is 4KB — avoid large stack allocations

## PR & commit conventions

- Branch from `main`; never push to `main` directly
- Conventional commits with scope, matching history: `feat(gateway/sparkplug_b): …`, `test(gateway): …`, `docs(odd): …`, `build(gateway): …`
- Branch naming: `feature/issue-NN-slug` or `feat/issue-NN-slug`
- Keep PRs ≤400 lines; chain larger work as one PR per slice
- Open PR via `gh pr create` once build and tests are green

## Security

- Never commit secrets — `.env`, `*.pem`, `*.key` are gitignored
- Local Wi-Fi credentials live in `firmware/gateway/components/wifi_manager/include/wifi_credentials.local.h` and must never be committed
- Never use unverified vendor electrical specs as design authority; mark them as pending verification

## Working agreements

- This is a teaching project: explain the reasoning, not just the patch, and point at official ESP-IDF examples
- Phases are sequential — do not pull features forward from later Fases
- Spec changes go through `openspec/` (Given/When/Then scenarios, RFC 2119 keywords); implementation plans go in `odd/tasks/`
- `project.md` and `docs/replanificacion/` are authoritative when they conflict with older phase docs
