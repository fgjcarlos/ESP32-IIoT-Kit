# Spec: iiot-kit-transformation

**Change**: iiot-kit-transformation
**Date**: 2026-05-23
**Artifact store**: openspec

---

## Domain: embedded-dashboard (NEW)

# Embedded Dashboard Specification

## Purpose

The ESP32-S3 gateway MUST serve a complete interactive dashboard without any external server. A Preact SPA is stored in SPIFFS and delivered to browsers over WiFi AP. REST API and WebSocket endpoints on the gateway are the sole data interfaces.

## Requirements

### Requirement: SPIFFS-Served Preact SPA

The documentation for Fase 4 MUST describe a Preact SPA built with Vite, gzip-compressed, and stored in the gateway's SPIFFS partition (~1.9 MB). The docs MUST specify a size budget (target ≤200 KB gzipped) and reference the `web/` source directory under `firmware/gateway/`.

#### Scenario: Developer reads Fase 4 for dashboard setup

- GIVEN a developer opens `Fases/fase-04-mqtt-dashboard.md`
- WHEN they read the embedded dashboard section
- THEN they find a Preact + Vite build pipeline explained
- AND a SPIFFS partition size budget is documented
- AND the docs reference `firmware/gateway/web/` as the SPA source directory

#### Scenario: Size budget exceeded scenario is addressed

- GIVEN the SPA source size exceeds the SPIFFS budget
- WHEN the developer reads the troubleshooting section
- THEN they find guidance on reducing bundle size (gzip, code splitting, asset limits)

### Requirement: REST API Documentation

The documentation MUST describe a REST API served by `esp_http_server` on the gateway covering at minimum: sensor data retrieval, node configuration, and OTA trigger endpoints. MQTT MUST be documented as optional, not required for basic operation.

#### Scenario: Developer designs a REST client for the gateway

- GIVEN a developer reads Fase 4 REST API section
- WHEN they look for endpoint definitions
- THEN they find at least GET /api/sensors, GET /api/nodes, POST /api/config, and POST /api/ota documented
- AND each endpoint shows request/response format

#### Scenario: Developer checks if MQTT is mandatory

- GIVEN a developer reads Fase 4
- WHEN they search for MQTT requirements
- THEN they find MQTT documented in a clearly labeled optional integration section
- AND the core dashboard workflow does not depend on MQTT being configured

### Requirement: WebSocket Real-Time Data Push

The documentation MUST describe a WebSocket endpoint on the gateway that pushes sensor events to connected browsers. The docs MUST specify the message format and connection lifecycle.

#### Scenario: Developer implements real-time sensor display

- GIVEN a developer reads the WebSocket section in Fase 4
- WHEN they look for how to connect and receive data
- THEN they find a WebSocket URL pattern, a message format (JSON), and reconnection guidance

#### Scenario: No active WebSocket client

- GIVEN no browser is connected to the WebSocket endpoint
- WHEN a sensor data message arrives at the gateway
- THEN the docs clarify that data is still processed and stored internally (WebSocket is push-only, not required for data collection)

---

## Domain: example-projects (NEW)

# Example Projects Specification

## Purpose

The `examples/` directory provides domain-specific implementations that use the generic IIoT platform. Fish farm is the first example. The structure MUST be self-contained and replicable for other domains.

## Requirements

### Requirement: Fish-Farm Example Directory

The repository MUST contain `examples/fish-farm/` with at least three files: `README.md` (overview and usage), `sensors.md` (pH, DO, ORP, turbidity sensor details), and `mqtt-topics.md` (fish-farm-specific topic hierarchy). All fish-farm-specific content removed from core docs MUST appear here.

#### Scenario: Developer with fish-farm use case finds domain content

- GIVEN a developer follows a cross-reference from Fase 3 pointing to `examples/fish-farm/`
- WHEN they open `examples/fish-farm/README.md`
- THEN they find sensor configuration details for pH, DO, ORP, and turbidity
- AND calibration procedures specific to aquaculture

#### Scenario: Core docs are free of fish-farm assumptions

- GIVEN a developer with a greenhouse monitoring use case reads Fases 0–6 and all Tutorials
- WHEN they search for "piscifactoria", "estanque", or "acuicultura"
- THEN zero matches are found outside `examples/fish-farm/`

### Requirement: DS18B20 as Universal Sensor Example

Fase 3 and Tutorial 3 MUST use the DS18B20 temperature sensor as the canonical worked example that all students implement, regardless of their target domain. Fish-farm sensor drivers (pH, DO, ORP, turbidity) MUST be removed from core Fase 3 content and referenced only via `examples/fish-farm/`.

#### Scenario: Student follows Fase 3 sensor implementation

- GIVEN a student opens `Fases/fase-03-sensores-actuadores.md`
- WHEN they implement the worked sensor example
- THEN the example uses DS18B20 temperature sensor
- AND there is no inline implementation of pH, DO, ORP, or turbidity sensors

---

## Domain: configurable-mqtt-namespace (NEW)

# Configurable MQTT Namespace Specification

## Purpose

The MQTT topic prefix MUST be configurable per deployment rather than hardcoded. The default namespace is `iiot-kit`. This enables any domain (fish farm, greenhouse, HVAC) to use distinct topic trees without code modification.

## Requirements

### Requirement: Configurable Namespace in Protocol Docs

The file `docs/replanificacion/02-protocolo-unificado.md` MUST document the MQTT topic namespace as a configurable NVS parameter named `mqtt_ns`, defaulting to `iiot-kit`. The topic pattern MUST be `{mqtt_ns}/{gateway_id}/...` and the prior hardcoded prefix `piscifactoria/` MUST be removed.

#### Scenario: Developer configures a custom namespace

- GIVEN a developer reads the MQTT namespace section in the protocol docs
- WHEN they set `mqtt_ns = "greenhouse"` in NVS
- THEN the docs show the resulting topic tree as `greenhouse/{gateway_id}/nodo/{nodo_id}/...`

#### Scenario: Default namespace used when not configured

- GIVEN a developer has not set `mqtt_ns` in NVS
- WHEN they read the default behavior docs
- THEN the docs state the default namespace is `iiot-kit`

### Requirement: Fish-Farm MQTT Topics in Example

The file `examples/fish-farm/mqtt-topics.md` MUST document the fish-farm-specific MQTT topic hierarchy using the configurable namespace pattern (e.g., with `mqtt_ns = "fish-farm"`).

#### Scenario: Fish-farm developer finds their MQTT topic structure

- GIVEN a developer opens `examples/fish-farm/mqtt-topics.md`
- WHEN they read the topic definitions
- THEN they find the full topic hierarchy using `fish-farm/{gateway_id}/...` as the prefix

---

## Domain: sensor-protocol (MODIFIED)

# Delta for Sensor Protocol

## MODIFIED Requirements

### Requirement: Sensor Type Enum

The `sensor_type_t` enum documented in `docs/replanificacion/02-protocolo-unificado.md` MUST include `SENSOR_TYPE_CUSTOM = 0xFF` as an escape hatch for user-defined sensor types. All existing values (TEMPERATURE=0x01, PH=0x02, DO=0x03, ORP=0x04, LEVEL=0x05, TURBIDITY=0x06) MUST be preserved unchanged. The `protocol_sensor_type_str()` function docs MUST specify that it returns `"CUSTOM"` for 0xFF.

(Previously: enum had no extension point; fish-farm types were the only defined values)

#### Scenario: Developer adds a CO2 sensor type

- GIVEN a developer reads the sensor type extension docs
- WHEN they want to add a CO2 sensor not in the enum
- THEN they use `SENSOR_TYPE_CUSTOM = 0xFF` in their sensor_type field
- AND the docs confirm the gateway logs it as "CUSTOM" without protocol errors

#### Scenario: Existing sensor type values are stable

- GIVEN existing nodes using TEMPERATURE (0x01) or PH (0x02)
- WHEN the updated protocol docs are consulted
- THEN the numeric values for all existing types are identical to the prior definition
- AND no wire-format migration is required

#### Scenario: values[] usage for CUSTOM type

- GIVEN a developer uses SENSOR_TYPE_CUSTOM
- WHEN they read the MSG_TYPE_DATA docs for values[] usage
- THEN they find guidance that up to 4 float values may carry custom readings and value_count MUST be set accurately

---

## Domain: tutorial-narrative (MODIFIED)

# Delta for Tutorial Narrative

## MODIFIED Requirements

### Requirement: Generic IIoT Framing Across All Docs

All 7 Fases (`fase-00` through `fase-06`) and all 7 Tutorials (`tutorial-00` through `tutorial-06`) MUST use generic IIoT language. The terms "piscifactoria", "estanque", "acuicultura", and "pez/peces" MUST NOT appear outside `examples/fish-farm/`. Domain-specific content MUST use "sensor node", "IIoT gateway", "deployment", or equivalent neutral terms.

(Previously: all narrative assumed fish-farm deployment as the primary context)

#### Scenario: Greenhouse developer reads Tutorial 01

- GIVEN a developer building a greenhouse monitoring system opens `Tutorial/tutorial-01-gateway-nucleo.md`
- WHEN they read through the tutorial
- THEN no fish-farm-specific language or assumptions appear in the core text
- AND any domain-example callouts are clearly labeled as optional examples

#### Scenario: Automated terminology check passes

- GIVEN the full repository minus `examples/fish-farm/`
- WHEN a grep is run for "piscifactoria|estanque|acuicultura"
- THEN zero matches are returned in `Fases/`, `Tutorial/`, `docs/`, `project.md`, `CLAUDE.md`, `README.md`, `mkdocs.yml`, `docs_site/`

### Requirement: Project Identity Files Updated

`project.md`, `CLAUDE.md`, `AI_CONTEXT.md`, and `README.md` MUST describe the project as "ESP32-IIoT-Kit" — a generic IIoT platform tutorial. The architecture diagram in `CLAUDE.md` MUST show embedded Preact SPA + REST + WebSocket as the primary dashboard, with no external server or React/Bun references.

(Previously: files described a fish-farm-specific system with external Bun/Node server and Vite+React dashboard)

#### Scenario: New contributor reads CLAUDE.md

- GIVEN a new contributor opens `CLAUDE.md`
- WHEN they read the Architecture Summary section
- THEN the diagram shows Preact SPA in SPIFFS, REST API, and WebSocket on the ESP32-S3
- AND no external server, SQLite, or Bun/Node is shown as a required component

---

## Domain: repo-rename (NEW)

# Repo Rename Specification

## Purpose

The GitHub repository MUST be renamed from `ESP32-fg` to `ESP32-IIoT-Kit`. All internal documentation references to the old repo name MUST be updated to the new name.

## Requirements

### Requirement: Repository Name Update

`mkdocs.yml`, `README.md`, `docs_site/index.md`, and any file containing `ESP32-fg` or `ESP32-fg` as a URL segment MUST be updated to reference `ESP32-IIoT-Kit`. The GitHub Pages URL changes from `fgjcarlos.github.io/ESP32-fg` to `fgjcarlos.github.io/ESP32-IIoT-Kit`.

#### Scenario: MkDocs site builds with correct URLs

- GIVEN `mkdocs.yml` is updated with the new repo name and site URL
- WHEN `mkdocs build` is run
- THEN the build succeeds with zero errors
- AND generated HTML contains no references to the old `ESP32-fg` URL

#### Scenario: ADR documents the transformation

- GIVEN `knowledge/Decisiones/004-iiot-kit-transformation.md` exists
- WHEN a developer reads it
- THEN they find the rationale for the rename, the trade-offs considered, and the date of the decision

### Requirement: MkDocs Site Name and Navigation

`mkdocs.yml` MUST set `site_name: ESP32-IIoT-Kit`, update `site_url` to the new GitHub Pages URL, and reflect the updated nav structure (Fase 4 renamed to "Dashboard Embebido + API REST").

#### Scenario: Landing page shows new project identity

- GIVEN a visitor opens the GitHub Pages site
- WHEN the index page loads
- THEN the site title reads "ESP32-IIoT-Kit" and the description describes a generic IIoT platform

---

## Acceptance Criteria (Automatable)

| Check | Command / Method | Pass Condition |
|-------|-----------------|----------------|
| AC-01: No fish-farm terms in core docs | `grep -r "piscifactoria\|estanque\|acuicultura" Fases/ Tutorial/ docs/ project.md CLAUDE.md README.md mkdocs.yml docs_site/` | Zero matches |
| AC-02: examples/fish-farm exists | `ls examples/fish-farm/{README.md,sensors.md,mqtt-topics.md}` | All 3 files present |
| AC-03: SENSOR_TYPE_CUSTOM in protocol docs | `grep "SENSOR_TYPE_CUSTOM" docs/replanificacion/02-protocolo-unificado.md` | At least 1 match with value `0xFF` |
| AC-04: Configurable MQTT namespace documented | `grep "mqtt_ns\|iiot-kit" docs/replanificacion/02-protocolo-unificado.md` | Matches for both `mqtt_ns` and `iiot-kit` default |
| AC-05: MkDocs build | `mkdocs build --strict` | Exit code 0 |
| AC-06: New knowledge files exist | `ls knowledge/Decisiones/004-iiot-kit-transformation.md knowledge/Conceptos/Preact.md knowledge/Conceptos/REST-API.md knowledge/Conceptos/WebSocket.md` | All 4 files present |
| AC-07: mkdocs.yml site name | `grep "site_name" mkdocs.yml` | Contains `ESP32-IIoT-Kit` |
| AC-08: CLAUDE.md no external server | `grep -i "bun\|node\.js\|sqlite\|express" CLAUDE.md` | Zero matches |
| AC-09: Fase 4 title updated | `grep "Dashboard Embebido" Fases/fase-04-mqtt-dashboard.md` | At least 1 match |
| AC-10: DS18B20 as primary in Fase 3 | `grep "DS18B20" Fases/fase-03-sensores-actuadores.md` | At least 1 match as primary example |

---

## Constraints

- Tutorial language: European Spanish (Spain) — all `.md` files in `Fases/`, `Tutorial/`, `docs/`, `knowledge/` are written in Spanish
- This change is DOCUMENTATION ONLY — no firmware source files (`.c`, `.h`, `CMakeLists.txt`) are created or modified
- All 13 replanning decisions (DC-01 to DC-13) remain valid and unchanged
- The ESP-NOW protocol wire format is unchanged: header is 13 bytes, all message struct sizes remain identical
- `PROTOCOL_VERSION` stays at 1; no version bump required for this documentation change
- The `examples/` directory contains documentation only — no firmware code

---

## Traceability Matrix

| Requirement | Proposal Capability | Proposal Section |
|-------------|---------------------|------------------|
| SPIFFS-Served Preact SPA | embedded-dashboard | New Capabilities |
| REST API Documentation | embedded-dashboard | New Capabilities |
| WebSocket Real-Time Data Push | embedded-dashboard | New Capabilities |
| Fish-Farm Example Directory | example-projects | New Capabilities |
| DS18B20 as Universal Sensor Example | example-projects | New Capabilities / Fase 3 |
| Configurable Namespace in Protocol Docs | configurable-mqtt-namespace | New Capabilities |
| Fish-Farm MQTT Topics in Example | configurable-mqtt-namespace | New Capabilities |
| Sensor Type Enum (CUSTOM=0xFF) | sensor-protocol | Modified Capabilities |
| Generic IIoT Framing Across All Docs | tutorial-narrative | Modified Capabilities |
| Project Identity Files Updated | tutorial-narrative | Modified Capabilities |
| Repository Name Update | repo-rename | Scope / File Change Matrix |
| MkDocs Site Name and Navigation | repo-rename | Scope / File Change Matrix |
