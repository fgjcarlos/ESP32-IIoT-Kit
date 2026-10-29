# AI Context: ESP32-IIoT-Kit — Generic IIoT Platform (ESP32)

## Project Overview

ESP32-IIoT-Kit is a generic IIoT monitoring and control platform built on ESP32 microcontrollers. It is an **educational project**: the goal is learning ESP-IDF and embedded systems by building something real.

The gateway is an **ESP32-S3-WROOM-1-N16R8**: 16 MB Quad-SPI flash and 8 MB Octal-SPI PSRAM. The connected unit's SoC and memory sizes are probe-confirmed (`docs/hardware/esp32-s3-wroom-1-n16r8.md`). Two things are **not** validated against the physical unit and must never be presented as facts:

- The **carrier pinout** comes from a user-supplied candidate image (GPIO0–GPIO21, GPIO35–GPIO48, 3V3/5V/GND/RST, GPIO0/BOOT, GPIO19/USB_D−, GPIO20/USB_D+, GPIO48/RGB_LED). It is not manufacturer documentation. The carrier model and revision are unknown.
- The **external display** is an 8-pin ST7789V2, 170(H) RGB × 320(V), 4-wire SPI panel of a nominal 1,9 in size. Its contact map (1 GND, 2 VCC, 3 SCL, 4 SDA, 5 RES, 6 DC, 7 CS, 8 BLK) is display-side, **not** a GPIO assignment. The reported 3,3 V, −20 to 70 °C, 12 o'clock viewing, two parallel white backlight LEDs and 20 mA operating current are seller/listing claims: the scope of the 20 mA is unknown (module vs. backlight vs. per-LED), it is not a backlight limit, and the reported supply does not establish logic input levels.

**Withdrawn value — never reuse:** the seller pixel pitch `0.1155 × 0.1155 mm` was withdrawn as incorrect. Do not use it, do not derive geometry from it, and do not infer a replacement pitch. The drawing-reported dimensions (AA 42.720 × 22.695 mm, LCD 48.520 × 24.800 mm, backlight 49.720 × 25.800 mm, PCB 62.000 × 29.000 mm, mounting spacing 58.000 × 25.000 mm) are **not** physical verification and must be compared with the exact delivered module before enclosure design.

Reserved on the confirmed N16R8 module: GPIO35, GPIO36 and GPIO37 are consumed by the Octal PSRAM and are unavailable for external use even though the candidate image labels them; GPIO48 drives the onboard WS2812 (functionally confirmed by an on-target smoke test); GPIO19/GPIO20 are native USB D−/D+. The LCD backlight candidate is **GPIO7**, provisional and **not approved for wiring**: GPIO22 does not exist on this module, which exposes IO0–21, IO35–42 and IO45–48. The candidate connection map, its reservations and its evidence status live in the canonical hardware profile — link to it instead of copying it.

## Sources of truth

Each topic has exactly one place where it is updated. Everything else points here instead of copying it.

| Topic | Source of truth |
|-------|-----------------|
| Gateway hardware, pinout, memory, partitions, display status | `docs/hardware/esp32-s3-wroom-1-n16r8.md` |
| Wire protocol | `docs/replanificacion/02-protocolo-unificado.md` |
| Corrected decisions (DC-01…DC-13) | `docs/replanificacion/01-decisiones-corregidas.md` |
| Corrected phases and schedule | `docs/replanificacion/03-fases-corregidas.md` |
| High-level architecture | `project.md` |
| What the firmware actually contains | `firmware/gateway/components/` and `AGENTS.md` |
| OTA security | `docs/ota/ota-security.md` |
| Change status | `openspec/changes/` and `openspec/changes/archive/` |
| Concept notes and ADRs | `knowledge/Conceptos/`, `knowledge/Decisiones/` |

The MkDocs site (`docs_site/`) is a **mirror made of symlinks** into `Tutorial/`, `Fases/` and `knowledge/`. Edit the canonical file; the mirror follows. Do not hand-edit `docs_site/` copies.

## Architecture Summary (target)

```
                   ┌──────────────────────────────────┐
                   │ ESP32-S3-WROOM-1-N16R8          │
                   │ (autonomous operation required)   │
                   │                                    │
                   │  ┌──────────────────────────────┐  │
  Browser ◄───────►│  │  Preact SPA (SPIFFS)         │  │
  (any device)     │  │  target ≤200KB gzipped       │  │
                   │  └──────────────────────────────┘  │
                   │  ┌──────────────────────────────┐  │
  REST API ◄──────►│  │  esp_http_server              │  │
                   │  │  Config + Data + OTA           │  │
                   │  └──────────────────────────────┘  │
                   │  ┌──────────────────────────────┐  │
  WebSocket ◄─────►│  │  Real-time data push          │  │
                   │  │  Sensor event stream           │  │
                   │  └──────────────────────────────┘  │
                   │  ┌──────────────────────────────┐  │
  MQTT (optional) ◄►  │  mqtt_bridge                  │  │
                   │  │  Cloud/LAN integration         │  │
                   │  └──────────────────────────────┘  │
                   │  ┌──────────────────────────────┐  │
  ESP-NOW ◄───────►│  │  espnow_manager               │  │
  (sensor nodes)   │  │  Backbone protocol             │  │
                   │  └──────────────────────────────┘  │
                   └──────────────────────────────────┘
                              │ ESP-NOW (2.4GHz, peer-to-peer)
                   ┌──────────┼──────────────┐
                   │          │              │
                Node A     Node B         Node C
                ESP32-C3   ESP32-C3       ESP32-C3
                (any sensor + Deep Sleep + ESP-NOW)
```

- **Gateway**: ESP32-S3-WROOM-1-N16R8, 16 MB Quad-SPI flash, 8 MB Octal-SPI PSRAM. Autonomous dashboard and OTA are requirements, not proven capabilities.
- **Sensor nodes**: ESP32-C3, 4 MB flash. Cycle: wake → read sensor → send ESP-NOW → sleep.
- **Embedded dashboard**: Preact SPA served from SPIFFS via `esp_http_server`, with a REST API and a WebSocket for data and config.
- **MQTT**: optional integration for cloud/LAN. Namespace configurable via NVS (default `iiot-kit`).

## Firmware state on disk

The gateway is modular: one ESP-IDF component per subsystem, each with `include/`, a `CMakeLists.txt` and Unity tests in `test/`. `main/` only composes. Do not describe a component as working when it is still a contract.

| Component | State |
|-----------|-------|
| `board_profile`, `board_rgb` | Implemented. Real flash/PSRAM sizing and the GPIO48 WS2812 LED. |
| `wifi_manager` | Implemented. AP+STA bring-up, deterministic helpers, credentials from the gitignored local header. |
| `ota_manifest`, `ota_manager` | Implemented. Signed manifest, compatibility/channel/anti-rollback checks, HTTPS download, digest re-read, `PENDING_VERIFY` with rollback. |
| `brand_profile`, `assets_brand` | Implemented. Build-time Kconfig choice plus versioned assets; secrets stay out of Git. |
| `sparkplug_b` | Implemented, **disabled by default** (`CONFIG_SPARKPLUG_B_ENABLED`, default `n`). Optional MQTT Sparkplug B v3.0.0 EoN-Node mode. |
| `espnow_manager`, `http_server` | Contract-only placeholders. The interface exists; the runtime does not. |
| `lcd_driver`, `lvgl_port`, `ui_screens`, `display_manager` | Contract-only placeholders, held for hardware verification. No `esp_lcd_*`, no `lv_init`, no draw buffers, no GPIO binding. |
| `mqtt_bridge` | Interface-only placeholder; stays a no-op while Sparkplug B is off. |

`firmware/node/` (ESP32-C3 nodes) and `firmware/gateway/web/` (Preact SPA) are **planned but not on disk** — do not assume they are runnable.

## Communication Protocols (priority order)

1. **ESP-NOW** (backbone): Node → gateway sensor data. Low latency (<10 ms), AES-128 encryption available, max 250 bytes/frame, max 20 peers.
2. **WiFi AP**: Local config, serves the embedded Preact SPA dashboard. Always active on the gateway.
3. **WiFi STA**: Internet/LAN for optional MQTT, OTA and SNTP. Reconnection with exponential backoff.

**Critical**: ESP-NOW and WiFi MUST use the same radio channel. The gateway fixes the WiFi channel and ESP-NOW uses that same channel.

## Current Phase

Fase 0 — Preparacion y aprendizaje (replanificacion aprobada). The gateway firmware foundation (issues #8–#14) is in place as the reference implementation; `firmware/node/` and the Preact SPA are not.

## Project Structure

```
ESP32-IIoT-Kit/
├── firmware/
│   ├── gateway/                  # ESP-IDF project for ESP32-S3 (target esp32s3)
│   │   ├── main/                 # app_main + nvs_config
│   │   ├── components/           # One component per subsystem (see the table above)
│   │   ├── test/                 # Top-level Unity test project
│   │   ├── partitions.csv        # Custom 16 MB layout
│   │   ├── sdkconfig.defaults    # esp32s3, Octal PSRAM, reproducible build, 8 KiB main stack
│   │   └── CMakeLists.txt
│   ├── node/                     # ESP-IDF project for ESP32-C3 — PLANNED, not on disk
│   │   ├── main/                 # main.c, espnow_node, sensor_manager, power_manager, nvs_config
│   │   ├── components/           # protocol (shared) + drivers (ds18b20 reference)
│   │   ├── partitions.csv
│   │   └── CMakeLists.txt
│   └── common/
│       └── protocol/             # Shared wire protocol (protocol.h/.c + host tests)
│
├── examples/fish-farm/           # Domain example on the same platform
├── Tutorial/                     # Tutorial docs (Fases 0-6)
├── Fases/                        # Phase reference docs
├── knowledge/                    # Obsidian learning vault
├── docs/hardware/                # Gateway hardware profile (source of truth)
├── docs/ota/                     # OTA security
├── docs/replanificacion/         # Corrected decisions, protocol, phases
├── docs_site/                    # MkDocs mirror (symlinks to the canonical sources)
├── openspec/                     # Spec-driven change workflow
├── odd/tasks/                    # Per-issue work plans
├── tools/                        # CI, host tests, OTA, brand, Sparkplug tooling
├── project.md                    # Architecture source of truth
├── AGENTS.md / CLAUDE.md         # AI context
└── README.md                     # Repository overview
```

## Coding Conventions

- **Language**: C11 for firmware, TypeScript/JSX for the Preact SPA (`firmware/gateway/web/`, not on disk yet)
- **Framework**: ESP-IDF v5.x (NOT Arduino)
- **Build**: CMake + `idf.py` for firmware; Vite for the Preact SPA
- **Naming**: `snake_case` for C functions/variables, module prefix (e.g. `wifi_manager_init()`, `espnow_manager_send()`)
- **Logging**: `esp_log` with module-specific tags. Use `ESP_LOGI`, `ESP_LOGW`, `ESP_LOGE`, `ESP_LOGD`. `printf` is forbidden.
- **Error handling**: `ESP_ERROR_CHECK()` for fatal errors, `esp_err_t` return codes for recoverable ones
- **NVS namespaces**: one per module (e.g. `"wifi"`, `"espnow"`, `"mqtt"`). NVS keys are limited to 15 characters — `ESP_ERR_NVS_KEY_TOO_LONG` is a real failure mode here.
- **Components**: one directory per reusable module, with `include/<name>.h`, `<name>.c`, `CMakeLists.txt`, `test/CMakeLists.txt` and `test/test_<name>.c`; register new components in `firmware/gateway/test/CMakeLists.txt` `TEST_COMPONENTS` with the defensive `list(FIND ...)` pattern
- **Public headers**: Doxygen `@brief`/`@return`; document hardware assumptions as `pending physical verification`

## Key Technical Constraints

| Constraint | Value |
|------------|-------|
| ESP-NOW max payload | 250 bytes per frame |
| ESP-NOW max encrypted peers | 20 |
| ESP-NOW + WiFi channel | Must be identical |
| Gateway flash | 16 MB Quad-SPI (ESP32-S3-WROOM-1-N16R8) |
| Gateway PSRAM | 8 MB Octal-SPI; GPIO35, GPIO36 and GPIO37 are unavailable for external use |
| Gateway partitions | 2 × 3 MiB OTA slots + 4 MiB SPIFFS (`firmware/gateway/partitions.csv`) |
| SPIFFS budget for the Preact SPA | Target ≤200 KB gzipped, hard limit ≤500 KiB |
| Provisional carrier pinout | User-supplied image, not manufacturer verification. GPIO35–37 unavailable (PSRAM); GPIO48 = RGB LED; GPIO19/20 = native USB while needed. |
| External display | 8-pin ST7789V2, 170(H) RGB × 320(V), 4-wire SPI, nominal 1,9 in — **pending physical verification**. Contacts are display-side, not GPIOs. The backlight candidate is GPIO7, provisional and not approved for wiring; the candidate map lives in the canonical hardware profile. |
| RGB565 framebuffer | 170 × 320 × 2 = 108,800 bytes (106.25 KiB); a PSRAM allocation of that size passed in the Unity app, but display DMA suitability and throughput are unmeasured |
| Node flash | 4 MB: 2 × 1.5 MB OTA + NVS |
| Node target battery life | 12+ months (wake every 5 min, 3400 mAh 18650) |
| FreeRTOS default task stack | 4 KB (avoid large stack allocations) |
| Main task stack | 8192 B — the 3584 B default overflows in `nvs_flash_init()` |

## Important ESP-IDF APIs

| Module | Key APIs |
|--------|----------|
| ESP-NOW | `esp_now_init`, `esp_now_register_recv_cb`, `esp_now_send`, `esp_now_add_peer` |
| WiFi | `esp_wifi_init`, `esp_wifi_set_mode(WIFI_MODE_APSTA)`, `esp_event_handler_register` |
| NVS | `nvs_flash_init`, `nvs_open`, `nvs_set_*`, `nvs_get_*`, `nvs_commit` |
| HTTP Server | `httpd_start`, `httpd_register_uri_handler`, `httpd_ws_recv_frame`, `httpd_ws_send_frame` |
| MQTT (optional) | `esp_mqtt_client_init`, `esp_mqtt_client_publish`, `esp_mqtt_client_subscribe` |
| Deep Sleep | `esp_deep_sleep_start`, `esp_sleep_enable_timer_wakeup` |
| OTA | `esp_ota_begin`, `esp_ota_write`, `esp_ota_end`, `esp_ota_set_boot_partition` |
| Time | `esp_sntp_init`, `esp_sntp_setservername` |
| SPIFFS | `esp_vfs_spiffs_register`, `esp_spiffs_info` |

## MQTT Topic Hierarchy

MQTT is optional. When enabled, the namespace is configurable via NVS key `mqtt_namespace` (default: `iiot-kit`).

### Fase 4: Flat MQTT (formative step)

```
{mqtt_ns}/{gateway_id}/nodo/{nodo_id}/temperatura
{mqtt_ns}/{gateway_id}/nodo/{nodo_id}/status
{mqtt_ns}/{gateway_id}/alertas
{mqtt_ns}/{gateway_id}/control/actuador/{id}
{mqtt_ns}/{gateway_id}/config/nodo/{nodo_id}
```

- Default `mqtt_ns`: `iiot-kit`
- For domain-specific topic hierarchies, see `examples/fish-farm/mqtt-topics.md`
- QoS 1 for sensor data, QoS 2 for actuator commands.

### Fase 6: MQTT Sparkplug B (optional integration)

Sparkplug B is an **optional** mode, never a required feature. The component exists at `firmware/gateway/components/sparkplug_b/` and is compiled in only with `CONFIG_SPARKPLUG_B_ENABLED=y` (default `n`); while it is off the gateway keeps working unchanged and `mqtt_bridge_start()` stays a no-op.

```
spBv1.0/{group_id}/NBIRTH/{gateway_id}
spBv1.0/{group_id}/NDEATH/{gateway_id}
spBv1.0/{group_id}/DBIRTH/{gateway_id}/{nodo_id}
spBv1.0/{group_id}/DDEATH/{gateway_id}/{nodo_id}
spBv1.0/{group_id}/DDATA/{gateway_id}/{nodo_id}
spBv1.0/{group_id}/NCMD/{gateway_id}
spBv1.0/{group_id}/DCMD/{gateway_id}/{nodo_id}
```

- Payloads encoded with Protobuf (nanopb on ESP32).
- Gateway maps as EoN Node, sensor nodes map as Devices.
- Birth/Death certificates provide automatic device state management.
- Conformance evidence: `docs/sparkplug_3.0.0-measurements.md`, `tools/sparkplug-conformance/`.

## ESP-NOW Message Protocol

```
Header (13 bytes):
  - msg_type:  1 byte  (sensor_data, ack, config, discovery)
  - node_id:   6 bytes (MAC address)
  - sequence:  2 bytes (packet loss detection)
  - timestamp: 4 bytes (seconds since epoch)

Payload: variable, max 230 bytes (sensor readings)
```

- Gateway sends ACK for each message. Node retries 3x if no ACK.
- 5 consecutive failures -> node enters power-save mode (30 min sleep).

## Common Mistakes to Watch For

1. Forgetting to init NVS before WiFi (WiFi stores calibration data in NVS)
2. Using different WiFi channels for ESP-NOW and WiFi (fails silently)
3. Not checking `esp_err_t` return values
4. Large stack allocations (FreeRTOS tasks default to 4KB stack)
5. Forgetting `nvs_commit()` after `nvs_set_*()`
6. Using `printf` instead of `ESP_LOGx` (loses tag filtering and level control)
7. Not running `idf.py set-target` before building (esp32s3 vs esp32c3)
8. Assuming regular RAM survives deep sleep (only RTC memory and NVS persist)
9. Raw ADC values without calibration (`esp_adc_cal` is required for accuracy)
10. Forgetting `source export.sh` before using `idf.py`
11. Preact SPA bundle exceeds SPIFFS budget (check gzipped size before flash; use `vite build --report`)
12. WebSocket handler not registered as `HTTPD_WS_TYPE_TEXT` in `httpd_uri_t` (causes connection drop)
13. NVS keys longer than 15 characters (returns `ESP_ERR_NVS_KEY_TOO_LONG` and silently kills the config load)
14. Reusing the withdrawn `0.1155 mm` pixel pitch, or deriving geometry from it
15. Treating the carrier pinout image or the seller display specs as verified hardware
16. Assuming a green Unity summary means a healthy run — the summary is printed before the FreeRTOS stack overflow detector fires, so an application that overflows the main task stack looks green. The whole suite runs in `app_main()`, so `CONFIG_ESP_MAIN_TASK_STACK_SIZE` has to hold the deepest single test, and `tools/ci/run-device-tests.sh` checks the headroom and fails on a crash

## How to Help the Developer

- This is an **educational/didactic project**. Explain WHY, not just HOW.
- Reference official ESP-IDF documentation and examples from `github.com/espressif/esp-idf/tree/master/examples/`.
- Follow the coding conventions above strictly.
- Guide toward the solution rather than writing everything directly.
- Always consider memory constraints and power consumption.
- The project phases are sequential. Do NOT suggest features from later phases.
- When in doubt, check `project.md` for the authoritative architecture and decisions.
- **Replanning**: corrections applied 2026-05-23. See `docs/replanificacion/` for corrected decisions, unified protocol, and phase adjustments.
- **IIoT transformation**: the project was rebranded from a fish-farm system to a generic IIoT platform (2026-05-23). Fish-farm-specific content lives in `examples/fish-farm/`. Core tutorial uses DS18B20 as the universal sensor example.
- **Obsidian vault**: learning documentation lives in `knowledge/`. Update concept notes and decision records as the project progresses.
- **Protocol source of truth**: `docs/replanificacion/02-protocolo-unificado.md` — NOT the original protocol definitions in phase files.
