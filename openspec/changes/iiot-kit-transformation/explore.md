# Exploration: Piscifactoria ESP32 → ESP32-IIoT-Kit

**Status**: done
**Date**: 2026-05-23

## Scope

Transform a fish-farm-specific ESP32 tutorial into a generic IIoT platform tutorial.
Fish farm becomes an example project. Core teaches the platform.

## Key Findings

### What's Generic (85-95% of content — KEEP)
- ESP-NOW protocol (header, ACK, DISCOVERY, CONFIG, OTA_PREPARE)
- Node firmware (deep sleep, RTC memory, sensor_manager interface)
- Gateway core (NVS, WiFi APSTA, HTTP server, ESP-NOW)
- OTA, security, testing concepts
- Sparkplug B migration plan
- All 13 replanning decisions (DC-01 to DC-13)

### What's Fish-Farm Specific (MOVE to examples/)
- sensor_type_t enum values (PH, DO, ORP, TURBIDITY)
- Sensor drivers (DS18B20 in water, pH probe, DO probe)
- MQTT topic prefix `piscifactoria/`
- Aquaculture-specific calibration procedures

### What's Architecturally Eliminated
- `server/` (Bun/Node + SQLite) — no external backend
- `dashboard/` (Vite + React) — replaced by embedded Preact on ESP32
- MQTT as primary data channel — becomes optional integration

### What's Added
- Embedded Preact SPA served from SPIFFS (1.9MB)
- Full REST API on ESP32 as primary interface
- WebSocket from ESP32 for real-time data
- `examples/` directory for domain-specific implementations

## Files Impact

| Category | Count | Examples |
|----------|-------|---------|
| Full rewrite | 3 | project.md, tutorial-04, fase-04 |
| Major update | 2 | tutorial-03, fase-03 |
| Moderate update | 6 | README, CLAUDE.md, mkdocs.yml, etc |
| Minor narrative | 10 | tutorial-00/01/02/05/06, fase-00/01/02/05/06 |
| New files | ~8 | examples/fish-farm/*, ADRs, concept notes |

## Recommended Approach: Hybrid (C)
Keep technical content, rewrite narrative shell and Fase 4 content.

## Open Questions
1. Repo rename: keep `ESP32-fg` or rename to `ESP32-IIoT-Kit`?
2. sensor_type_t strategy: keep fish-farm enum as examples + add CUSTOM, or redesign?
3. Fase 3 pedagogy: DS18B20 as universal example, or fish-farm as worked example?
