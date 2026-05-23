# AI Context: Smart Fish Farm Monitoring System (ESP32)

## Project Overview

An autonomous monitoring and control system for fish farms built on ESP32 microcontrollers. Uses ESP-NOW for low-power sensor node communication, WiFi AP for local configuration, and WiFi STA for cloud/LAN integration via MQTT. This is an **educational project** — the goal is learning ESP-IDF and embedded systems development.

## Architecture Summary

```
Cloud/LAN: Mosquitto (MQTT) <-> Bun/Node API + SQLite <-> Vite + React Dashboard
                |
          MQTT (WiFi STA)
                |
         ┌──────────────┐
         │   GATEWAY     │ <-- WiFi AP (embedded HTML/JS dashboard)
         │   ESP32-S3    │     Firmware: C (ESP-IDF v5.x)
         │   8MB Flash   │     ESP-NOW + WiFi AP/STA + MQTT + HTTP
         └──────┬───────┘
                │ ESP-NOW (2.4GHz, peer-to-peer)
         ┌──────┼──────────┐
         │      │          │
      Node A  Node B    Node C
      ESP32-C3 (4MB Flash each)
      Sensors + Deep Sleep + ESP-NOW
```

- **Gateway**: ESP32-S3, 8MB flash, 2MB PSRAM. Runs WiFi APSTA + ESP-NOW simultaneously.
- **Sensor nodes**: ESP32-C3, 4MB flash. Deep sleep cycle: wake -> read sensor -> send ESP-NOW -> sleep.
- **Server**: Bun (or Node.js) + TypeScript, Mosquitto MQTT broker, SQLite (-> InfluxDB later).
- **Dashboard**: Vite + React + Tailwind + Recharts. Real-time via WebSocket.
- **Embedded dashboard**: HTML + CSS + vanilla JS served from gateway flash (emergency/config panel).

## Communication Protocols (priority order)

1. **ESP-NOW** (backbone): Node -> gateway sensor data. Low latency (<10ms), AES-128 encryption. Max 250 bytes/frame, 20 encrypted peers.
2. **WiFi AP**: Local config, embedded emergency dashboard. Always active on gateway.
3. **WiFi STA**: Internet/LAN for MQTT, OTA, SNTP. Reconnection with exponential backoff.

**Critical**: ESP-NOW and WiFi MUST use the same radio channel. Gateway fixes the WiFi channel and ESP-NOW uses that same channel.

## Current Phase

> **Update this manually**: Fase 0 - Preparacion y aprendizaje (pre-inicio, replanificacion completada 2026-05-23)

## Project Structure

```
piscifactoria/
├── firmware/
│   ├── gateway/              # ESP-IDF project for ESP32-S3
│   │   ├── main/
│   │   │   ├── main.c
│   │   │   ├── wifi_manager.c/.h
│   │   │   ├── espnow_manager.c/.h
│   │   │   ├── mqtt_bridge.c/.h
│   │   │   ├── http_server.c/.h
│   │   │   ├── ota_manager.c/.h
│   │   │   ├── nvs_config.c/.h
│   │   │   └── actuator_ctrl.c/.h
│   │   ├── components/
│   │   │   └── protocol/     # Shared messaging protocol
│   │   ├── web/              # Embedded HTML/CSS/JS
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
│           ├── protocol.h
│           └── protocol.c
│
├── server/                   # Bun/Node backend (TypeScript)
├── dashboard/                # Vite + React frontend
└── docs/                     # Project documentation
```

## Coding Conventions

- **Language**: C11 for firmware, TypeScript for server/dashboard
- **Framework**: ESP-IDF v5.x (NOT Arduino)
- **Build**: CMake + `idf.py`
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
| Gateway flash | 8MB: 2x 3MB OTA + 1.9MB SPIFFS (web) + NVS |
| Node flash | 4MB: 2x 1.5MB OTA + NVS |
| Node target battery life | 12+ months (wake every 5 min, 3400mAh 18650) |
| FreeRTOS default task stack | 4KB (avoid large stack allocations) |

## Important ESP-IDF APIs

| Module | Key APIs |
|--------|----------|
| ESP-NOW | `esp_now_init`, `esp_now_register_recv_cb`, `esp_now_send`, `esp_now_add_peer` |
| WiFi | `esp_wifi_init`, `esp_wifi_set_mode(WIFI_MODE_APSTA)`, `esp_event_handler_register` |
| NVS | `nvs_flash_init`, `nvs_open`, `nvs_set_*`, `nvs_get_*`, `nvs_commit` |
| HTTP Server | `httpd_start`, `httpd_register_uri_handler` |
| MQTT | `esp_mqtt_client_init`, `esp_mqtt_client_publish`, `esp_mqtt_client_subscribe` |
| Deep Sleep | `esp_deep_sleep_start`, `esp_sleep_enable_timer_wakeup` |
| OTA | `esp_ota_begin`, `esp_ota_write`, `esp_ota_end`, `esp_ota_set_boot_partition` |
| Time | `esp_sntp_init`, `esp_sntp_setservername` |

## MQTT Topic Hierarchy

### Fase 4: MQTT plano (paso formativo)

```
piscifactoria/{gateway_id}/nodo/{nodo_id}/temperatura
piscifactoria/{gateway_id}/nodo/{nodo_id}/status
piscifactoria/{gateway_id}/alertas
piscifactoria/{gateway_id}/control/actuador/{id}
piscifactoria/{gateway_id}/config/nodo/{nodo_id}
```

- QoS 1 for sensor data, QoS 2 for actuator commands.

### Fase 6: MQTT Sparkplug B (objetivo final)

```
spBv1.0/piscifactoria/NBIRTH/{gateway_id}
spBv1.0/piscifactoria/NDEATH/{gateway_id}
spBv1.0/piscifactoria/DBIRTH/{gateway_id}/{nodo_id}
spBv1.0/piscifactoria/DDEATH/{gateway_id}/{nodo_id}
spBv1.0/piscifactoria/DDATA/{gateway_id}/{nodo_id}
spBv1.0/piscifactoria/NCMD/{gateway_id}
spBv1.0/piscifactoria/DCMD/{gateway_id}/{nodo_id}
```

- Payloads encoded with Protobuf (nanopb on ESP32, protobufjs on server).
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

## How to Help the Developer

- This is an **educational/didactic project**. Explain WHY, not just HOW.
- Reference official ESP-IDF documentation and examples from `github.com/espressif/esp-idf/tree/master/examples/`.
- Follow the coding conventions above strictly.
- Guide toward the solution rather than writing everything directly.
- Always consider memory constraints and power consumption.
- The project phases are sequential. Do NOT suggest features from later phases.
- When in doubt, check `project.md` for the authoritative architecture and decisions.
- **Replanning**: Corrections applied 2026-05-23. See `docs/replanificacion/` for corrected decisions, unified protocol, and phase adjustments.
- **Obsidian vault**: Learning documentation lives in `knowledge/`. Update concept notes and decision records as the project progresses.
- **Protocol source of truth**: `docs/replanificacion/02-protocolo-unificado.md` — NOT the original protocol definitions in phase files.
