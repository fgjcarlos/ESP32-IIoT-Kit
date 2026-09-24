# AI Context: ESP32-IIoT-Kit — Generic IIoT Platform (ESP32)

## Project Overview

A generic IIoT monitoring and control platform built on ESP32 microcontrollers. The selected gateway candidate is an ESP32-S3-WROOM-1-N16R8 module/carrier with an external display that the user confirms has 8 pins, an ST7789V2 controller, 170(H) RGB × 320(V) resolution, a 4-wire SPI interface, and a nominal 1.9 in size; it is not hardware-validated. User/listing-reported, unverified display specifications are 3.3 V operating voltage, −20 to 70 °C operating temperature, 12 o'clock viewing direction, two white backlight LEDs in parallel, and 20 mA operating current. The 20 mA scope is explicitly unknown: it is not established as whole-module, backlight, per-LED, or total-backlight current. **Warning — withdrawn seller value:** the user withdrew the incorrect `0.1155 × 0.1155 mm` pixel-pitch value; do not use it or geometry derived from it, and do not infer a replacement pitch. The latest user-provided mechanical drawing reports AA 42.720 × 22.695 mm, LCD outline 48.520 × 24.800 mm, backlight outline 49.720 × 25.800 mm, PCB outline 62.000 × 29.000 mm, and mounting-hole center spacing 58.000 × 25.000 mm. These dimensions are not physically verified and must be compared with the exact delivered 8-pin module before enclosure design. Do not infer logic input levels from the reported 3.3 V supply, invent a backlight driver/current-limit circuit, or use specifications from the separate 30-pin display variant. A user-supplied provisional carrier pinout image labels GPIO0–GPIO21 and GPIO35–GPIO48, 3V3/5V/GND/RST, GPIO0 as BOOT, GPIO19 as USB_D−, GPIO20 as USB_D+, and GPIO48 as RGB_LED. This is not verified manufacturer documentation: confirm the exact carrier model, revision, and physical unit before any GPIO assignment. On the confirmed N16R8 module, GPIO35, GPIO36, and GPIO37 are occupied by Octal PSRAM and unavailable for external use despite appearing on the image. Reserve GPIO48 for the onboard RGB LED and GPIO19/GPIO20 for native USB while it is needed; do not propose them for the external LCD without a deliberate future decision. A separate user-supplied display pinout image identifies the display contacts as 1 GND, 2 VCC, 3 SCL, 4 SDA, 5 RES, 6 DC, 7 CS, and 8 BLK; these are display contact numbers, not ESP32 GPIO assignments or independent manufacturer verification. The S3 carrier GPIO mapping, VCC and logic voltage/safe levels, reset polarity, backlight current/control, and physical label verification remain unresolved. The gateway requirement is autonomous operation: an embedded Preact SPA served from SPIFFS, a REST API, and a WebSocket server — no external server required. Sensor nodes (ESP32-C3) use ESP-NOW and deep sleep for maximum battery life. MQTT is an optional cloud/LAN integration. This is an **educational project** — the goal is learning ESP-IDF and embedded systems development.

## Architecture Summary

```
                   ┌──────────────────────────────────┐
                   │ ESP32-S3-WROOM-1-N16R8 CANDIDATE  │
                   │ (Autonomous operation required)   │
                   │                                    │
                   │  ┌──────────────────────────────┐  │
  Browser ◄───────►│  │  Preact SPA (SPIFFS)         │  │
  (any device)     │  │  ~200 KB gzipped             │  │
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

- **Gateway**: selected ESP32-S3-WROOM-1-N16R8 module/carrier candidate, 16 MB Quad-SPI flash and 8 MB Octal-SPI PSRAM. Carrier wiring and the external display remain unverified. Autonomous dashboard and OTA remain requirements, not proven capabilities.
- **Sensor nodes**: ESP32-C3, 4MB flash. Deep sleep cycle: wake -> read sensor -> send ESP-NOW -> sleep.
- **Embedded dashboard**: Preact SPA (~200KB gzipped) served from SPIFFS via `esp_http_server`. REST API + WebSocket for data and config.
- **MQTT (optional)**: `mqtt_bridge` for cloud/LAN integration. Configurable namespace via NVS (default: `iiot-kit`).

## Communication Protocols (priority order)

1. **ESP-NOW** (backbone): Node -> gateway sensor data. Low latency (<10ms), AES-128 encryption. Max 250 bytes/frame, 20 peers.
2. **WiFi AP**: Local config, serves embedded Preact SPA dashboard. Always active on gateway.
3. **WiFi STA**: Internet/LAN for optional MQTT, OTA, SNTP. Reconnection with exponential backoff.

**Critical**: ESP-NOW and WiFi MUST use the same radio channel. Gateway fixes the WiFi channel and ESP-NOW uses that same channel.

## Current Phase

> **Update this manually**: Fase 0 - Preparacion y aprendizaje (replanificacion aprobada, listo para implementacion)

## Project Structure

```
ESP32-IIoT-Kit/
├── firmware/
│   ├── gateway/              # ESP-IDF project for ESP32-S3
│   │   ├── main/
│   │   │   ├── main.c
│   │   │   ├── wifi_manager.c/.h
│   │   │   ├── espnow_manager.c/.h
│   │   │   ├── mqtt_bridge.c/.h      # Optional MQTT integration
│   │   │   ├── http_server.c/.h      # REST API + WebSocket
│   │   │   ├── ota_manager.c/.h
│   │   │   ├── nvs_config.c/.h
│   │   │   └── actuator_ctrl.c/.h
│   │   ├── components/
│   │   │   └── protocol/     # Shared messaging protocol
│   │   ├── web/              # Preact SPA source (build → SPIFFS)
│   │   │   ├── src/
│   │   │   │   ├── app.jsx
│   │   │   │   ├── components/
│   │   │   │   └── hooks/
│   │   │   ├── package.json
│   │   │   └── vite.config.js
│   │   ├── partitions.csv
│   │   └── CMakeLists.txt
│   │
│   ├── node/                 # ESP-IDF project for ESP32-C3
│   │   ├── main/
│   │   │   ├── main.c
│   │   │   ├── espnow_node.c/.h
│   │   │   ├── sensor_manager.c/.h
│   │   │   ├── power_manager.c/.h
│   │   │   └── nvs_config.c/.h
│   │   ├── components/
│   │   │   ├── protocol/     # Same shared component
│   │   │   └── drivers/      # Sensor drivers
│   │   ├── partitions.csv
│   │   └── CMakeLists.txt
│   │
│   └── common/               # Shared code (protocol, types)
│       └── protocol/
│           ├── protocol.h    # + SENSOR_TYPE_CUSTOM = 0xFF
│           └── protocol.c
│
├── examples/                 # Domain-specific examples
│   └── fish-farm/
│       ├── README.md
│       ├── sensors.md
│       └── mqtt-topics.md
│
├── Tutorial/                 # Tutorial docs (Fases 0-6)
├── Fases/                    # Phase reference docs
├── knowledge/                # Obsidian learning vault
└── docs/                     # Internal docs and replanning
```

## Coding Conventions

- **Language**: C11 for firmware, TypeScript/JSX for Preact SPA (`firmware/gateway/web/`)
- **Framework**: ESP-IDF v5.x (NOT Arduino)
- **Build**: CMake + `idf.py` for firmware; Vite for Preact SPA
- **Naming**: `snake_case` for C functions/variables, module prefix (e.g., `wifi_manager_init()`, `espnow_manager_send()`)
- **Logging**: `esp_log` with module-specific tags. Use `ESP_LOGI`, `ESP_LOGW`, `ESP_LOGE`, `ESP_LOGD`.
- **Error handling**: `ESP_ERROR_CHECK()` for fatal errors, `esp_err_t` return codes for recoverable ones.
- **NVS namespaces**: One per module (e.g., `"wifi"`, `"espnow"`, `"sensor"`, `"mqtt"`)
- **Components**: Use ESP-IDF component structure for reusable modules

## Key Technical Constraints

| Constraint | Value |
|------------|-------|
| ESP-NOW max payload | 250 bytes per frame |
| ESP-NOW max encrypted peers | 20 |
| ESP-NOW + WiFi channel | Must be identical |
| Gateway flash | 16 MB Quad-SPI (ESP32-S3-WROOM-1-N16R8); final allocation remains pending ODD-3 |
| Gateway PSRAM | 8 MB Octal-SPI; GPIO35, GPIO36, and GPIO37 are unavailable for external use |
| Provisional carrier pinout | User-supplied image labels GPIO0–GPIO21 and GPIO35–GPIO48, 3V3/5V/GND/RST, GPIO0/BOOT, GPIO19/USB_D−, GPIO20/USB_D+, and GPIO48/RGB_LED. It is not manufacturer verification; validate the exact carrier model, revision, and physical unit before assigning GPIOs. GPIO35–GPIO37 remain unavailable due to Octal PSRAM; reserve GPIO48 for RGB LED and GPIO19/GPIO20 for native USB while needed. |
| External display | User-supplied contact map: 1 GND, 2 VCC, 3 SCL, 4 SDA, 5 RES, 6 DC, 7 CS, 8 BLK; these are display contacts, not ESP32 GPIOs. The user reaffirms a nominal 1.9 in size. The latest user-provided drawing reports AA 42.720 × 22.695 mm, LCD outline 48.520 × 24.800 mm, backlight outline 49.720 × 25.800 mm, PCB outline 62.000 × 29.000 mm, and mounting-hole center spacing 58.000 × 25.000 mm; these are not physically verified and require comparison with the exact delivered module before enclosure design. **Warning — withdrawn seller value:** `0.1155 × 0.1155 mm` pixel pitch is incorrect; do not use it or its derived geometry, and do not infer a replacement. Reported 3.3 V operating voltage does not establish logic input levels. The reported 20 mA has an explicitly unknown scope and does not establish per-LED or total-backlight current; backlight current/control and all electrical behavior remain unresolved. |
| RGB565 framebuffer calculation | 170 × 320 × 2 = 108,800 bytes (106.25 KiB); runtime feasibility remains unmeasured |
| SPIFFS budget for Preact SPA | Target ≤200KB gzipped; final partition size remains pending ODD-3 |
| Node flash | 4MB: 2x 1.5MB OTA + NVS |
| Node target battery life | 12+ months (wake every 5 min, 3400mAh 18650) |
| FreeRTOS default task stack | 4KB (avoid large stack allocations) |

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

### Fase 6: MQTT Sparkplug B (future goal)

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

## How to Help the Developer

- This is an **educational/didactic project**. Explain WHY, not just HOW.
- Reference official ESP-IDF documentation and examples from `github.com/espressif/esp-idf/tree/master/examples/`.
- Follow the coding conventions above strictly.
- Guide toward the solution rather than writing everything directly.
- Always consider memory constraints and power consumption.
- The project phases are sequential. Do NOT suggest features from later phases.
- When in doubt, check `project.md` for the authoritative architecture and decisions.
- **Replanning**: Corrections applied 2026-05-23. See `docs/replanificacion/` for corrected decisions, unified protocol, and phase adjustments.
- **IIoT transformation**: The project was rebranded from a fish-farm system to a generic IIoT platform (2026-05-23). Fish-farm-specific content lives in `examples/fish-farm/`. Core tutorial uses DS18B20 as the universal sensor example.
- **Obsidian vault**: Learning documentation lives in `knowledge/`. Update concept notes and decision records as the project progresses.
- **Protocol source of truth**: `docs/replanificacion/02-protocolo-unificado.md` — NOT the original protocol definitions in phase files.
