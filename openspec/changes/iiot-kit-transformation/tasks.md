# Tasks: iiot-kit-transformation

**Change**: iiot-kit-transformation
**Date**: 2026-05-23
**Artifact store**: openspec
**Total tasks**: 22
**Delivery strategy**: ask-on-risk

---

## Processing Model

Tasks are grouped into 6 waves matching the design document. Within a wave, tasks are **sequential by default** unless marked `[PARALLEL]`. Waves themselves are sequential (each wave depends on the prior wave completing first), with two exceptions: Waves 2 and 3 can start in parallel once Wave 1 is complete.

```
Wave 1 (Anchors)
    ├── Wave 2 (Fase 4 rewrite) ─┐ both start after Wave 1 completes
    └── Wave 3 (Fase 3 + Protocol) ─┘
Wave 4 (New files — parallelizable after Wave 3 completes)
Wave 5 (Narrative sweep — after Wave 4 completes)
Wave 6 (Verification — after Wave 5 completes)
```

---

## Wave 1 — Anchor Files (Sequential, do first)

These three files define the new vocabulary and architecture. All subsequent files reference them.

---

### [x] T-01 — Rewrite project.md

**Spec requirements**: Generic IIoT Framing (tutorial-narrative), Project Identity Files Updated (tutorial-narrative)

**Description**: Full rewrite of `project.md` to describe ESP32-IIoT-Kit as a generic IIoT platform. Remove the fish-farm framing, external server stack (Bun/Node, SQLite, Mosquitto), and React dashboard. Replace the architecture section with the autonomous ESP32-S3 gateway diagram (Preact SPA in SPIFFS + REST API + WebSocket + optional MQTT). Replace all fish-farm vocabulary with generic IIoT terms per the replacement table in the design doc. Remove references to the `server/` and `dashboard/` directories as planned deliverables.

**Files affected**:
- `/home/composedof2/Dev/ESP32-fg/project.md` (481 lines — full rewrite)

**Dependencies**: None (first task)

**Estimated effort**: 2.5 h

**Acceptance criteria**:
- `grep -i "piscifactoria\|estanque\|acuicultura\|bun\|sqlite\|mosquitto" project.md` → zero matches
- Architecture section shows embedded Preact SPA, REST API, WebSocket, optional MQTT
- No reference to `server/` or `dashboard/` directories as implementation targets
- Project name is "ESP32-IIoT-Kit" throughout

---

### [x] T-02 — Major update CLAUDE.md

**Spec requirements**: Project Identity Files Updated (tutorial-narrative), SPIFFS-Served Preact SPA (embedded-dashboard), REST API Documentation (embedded-dashboard), WebSocket Real-Time Data Push (embedded-dashboard)

**Description**: Update `CLAUDE.md` to reflect the new architecture. Replace the Architecture Summary section: the diagram MUST show embedded Preact SPA + REST + WebSocket as primary dashboard with MQTT as optional. Remove all external server references (Bun/Node, SQLite, React, Mosquitto as required components). Update the Key Technical Constraints table (remove server-side constraints, add SPIFFS budget constraint for Preact SPA). Update Important ESP-IDF APIs table to include `esp_http_server` WebSocket and any Preact/Vite references. Update Project Structure section to show `firmware/gateway/web/` (Preact source) and `examples/` directory. Replace fish-farm language throughout.

**Files affected**:
- `/home/composedof2/Dev/ESP32-fg/CLAUDE.md` (194 lines — major update, not full rewrite)

**Dependencies**: T-01 (CLAUDE.md references project.md architecture)

**Estimated effort**: 1.5 h

**Acceptance criteria**:
- `grep -i "bun\|node\.js\|sqlite\|express" CLAUDE.md` → zero matches (AC-08)
- Architecture Summary diagram shows Preact SPA, REST API, WebSocket, optional MQTT
- No fish-farm language in CLAUDE.md
- `firmware/gateway/web/` appears in project structure
- `examples/` directory appears in project structure

---

### [x] T-03 — Major update AI_CONTEXT.md

**Spec requirements**: Project Identity Files Updated (tutorial-narrative)

**Description**: Mirror CLAUDE.md changes in `AI_CONTEXT.md`. This file is used for AI context and must stay in sync with CLAUDE.md. Apply identical architecture diagram update, remove external server references, replace fish-farm language, update project structure. If content is substantially duplicated between the two files, evaluate whether `AI_CONTEXT.md` should be replaced by a short note pointing readers to CLAUDE.md instead (this is an editorial decision — choose the option that reduces maintenance burden; document the choice in a comment).

**Files affected**:
- `/home/composedof2/Dev/ESP32-fg/AI_CONTEXT.md` (173 lines — major update)

**Dependencies**: T-02 (must mirror CLAUDE.md)

**Estimated effort**: 1 h

**Acceptance criteria**:
- `grep -i "piscifactoria\|bun\|sqlite" AI_CONTEXT.md` → zero matches
- Architecture references in AI_CONTEXT.md are consistent with CLAUDE.md
- File either updated to match or merged with a redirect comment

---

## Wave 2 — Fase 4 Full Rewrite (Can start after Wave 1; parallel with Wave 3)

The largest single change. Entirely replaces the old Bun/React/MQTT dashboard architecture.

---

### [x] T-04 — Rewrite Fases/fase-04-mqtt-dashboard.md

**Spec requirements**: SPIFFS-Served Preact SPA (embedded-dashboard), REST API Documentation (embedded-dashboard), WebSocket Real-Time Data Push (embedded-dashboard), Generic IIoT Framing (tutorial-narrative), MkDocs Site Name and Navigation (repo-rename)

**Description**: Full rewrite of the 950-line Fase 4 doc. New title: "Fase 4: Dashboard Embebido + API REST". Structure follows the design's new Fase 4 outline exactly:

- **Sub-task 4.1: REST API on ESP32** (3 tasks):
  - T4.1.1: REST endpoint categories with `esp_http_server`: `/api/status`, `/api/nodes`, `/api/nodes/{id}/readings`, `/api/config`, `/api/actuators` — each endpoint must show request/response format
  - T4.1.2: JSON serialization with cJSON for API responses
  - T4.1.3: WebSocket handler on `esp_http_server` for real-time sensor data push; include WebSocket URL pattern, JSON message format, and reconnection guidance

- **Sub-task 4.2: Embedded Preact SPA** (3 tasks):
  - T4.2.1: Scaffold Preact + Vite project in `firmware/gateway/web/`; configure Vite for gzip output targeting SPIFFS; document size budget (target ≤200 KB gzipped, hard limit 1.9MB SPIFFS)
  - T4.2.2: SPA pages: dashboard (real-time node cards), config (WiFi, thresholds), status (gateway health)
  - T4.2.3: SPIFFS serving integration; CMake build hook that builds Preact then flashes to SPIFFS partition; include troubleshooting guidance for bundle size overruns

- **Sub-task 4.3: MQTT as Optional Integration** (2 tasks):
  - T4.3.1: Refactor mqtt_bridge docs as optional module; configurable namespace (`{namespace}/{gateway_id}/...` stored in NVS key `mqtt_namespace`, default `iiot-kit`)
  - T4.3.2: Document MQTT topics with generic namespace; reference `examples/fish-farm/mqtt-topics.md` for fish-farm-specific topics

- **Sub-task 4.4: Integration and Verification** (2 tasks):
  - T4.4.1: End-to-end flow: node sensor → ESP-NOW → gateway → REST/WebSocket → browser
  - T4.4.2: Size budget verification: Preact SPA must be under 500KB gzipped; document how to measure

**Files affected**:
- `/home/composedof2/Dev/ESP32-fg/Fases/fase-04-mqtt-dashboard.md` (950 lines — full rewrite, expect ~700-900 lines output)

**Dependencies**: T-01 (defines vocabulary), T-02 (defines architecture)

**Estimated effort**: 3 h

**Acceptance criteria**:
- `grep "Dashboard Embebido" Fases/fase-04-mqtt-dashboard.md` → at least 1 match (AC-09)
- `grep -i "piscifactoria\|estanque\|acuicultura" Fases/fase-04-mqtt-dashboard.md` → zero matches
- REST endpoints GET /api/sensors (or /api/nodes), GET /api/nodes, POST /api/config, POST /api/ota are documented with request/response format
- MQTT section is clearly labeled as optional integration
- WebSocket URL pattern, JSON message format, and reconnection guidance are present
- SPIFFS size budget (≤200 KB gzipped) is documented
- Troubleshooting section covers bundle size reduction

---

### [x] T-05 — Rewrite Tutorial/tutorial-04-mqtt-dashboard.md

**Spec requirements**: SPIFFS-Served Preact SPA (embedded-dashboard), REST API Documentation (embedded-dashboard), WebSocket Real-Time Data Push (embedded-dashboard), Generic IIoT Framing (tutorial-narrative)

**Description**: Full rewrite of the 454-line Tutorial 4 doc. Teach the concepts behind the new Fase 4 architecture. Follow the design's "New Tutorial 4 Structure" exactly. Six concept blocks to cover:

1. Why embedded dashboards beat external servers for IIoT (autonomy, offline capability, single-device deployment) — use construction/architecture analogy
2. REST API design principles on constrained devices (resource naming, HTTP status codes, cJSON usage, request/response format)
3. WebSocket: bidirectional real-time communication (contrast with polling and SSE; connection lifecycle, reconnection)
4. Preact: React-like development at 3KB — why not React (too large for SPIFFS), why not vanilla JS (maintainability)
5. SPIFFS: serving static assets from flash (partition layout, size budgets, gzip workflow)
6. MQTT as optional cloud integration (publish/subscribe review, configurable namespace, when to use it)

Language: European Spanish (Spain). Educational and didactic tone. Guide the reader toward understanding WHY, not just HOW.

**Files affected**:
- `/home/composedof2/Dev/ESP32-fg/Tutorial/tutorial-04-mqtt-dashboard.md` (454 lines — full rewrite, expect ~400-500 lines output)

**Dependencies**: T-04 (tutorial must align with the Fase doc it explains)

**Estimated effort**: 2.5 h

**Acceptance criteria**:
- `grep -i "piscifactoria\|estanque\|acuicultura\|bun\|sqlite" Tutorial/tutorial-04-mqtt-dashboard.md` → zero matches
- All 6 concept blocks are present and substantive (not just headings)
- MQTT section is explicitly framed as optional
- Preact size rationale (~3KB) is explained
- WebSocket vs polling tradeoff is explained

---

## Wave 3 — Fase 3 Restructure + Protocol (Can start after Wave 1; parallel with Wave 2)

---

### [x] T-06 — Major update Fases/fase-03-sensores-actuadores.md

**Spec requirements**: DS18B20 as Universal Sensor Example (example-projects), Generic IIoT Framing (tutorial-narrative)

**Description**: Major structural update of the 950-line Fase 3 doc. Per the design's "Fase 3 Restructuring" section:

**Keep in core doc**:
- Sub-task 3.1 (DS18B20 driver + integration): keep entirely — this becomes THE universal sensor example all students implement
- Sub-task 3.5 (validation + filtering): keep — domain-agnostic signal processing
- Sub-task 3.6 (actuators + alerts): keep — generic relay control, threshold alerts with hysteresis

**Move to examples/fish-farm/sensors.md** (do not delete yet — reference them):
- Sub-task 3.2 (pH driver + calibration): all 3 tasks
- Sub-task 3.3 (DO driver + temperature compensation): both tasks
- Sub-task 3.4 (ORP + ultrasonic level): both tasks

After removing fish-farm tasks, add a cross-reference callout at the end of the sensor sub-tasks section: "Para implementar sensores específicos del dominio como pH, oxígeno disuelto o nivel de agua, consulta los ejemplos en `examples/fish-farm/sensors.md`."

DS18B20 MUST appear as the primary worked example. Replace all fish-farm narrative language per the replacement vocabulary table.

**Files affected**:
- `/home/composedof2/Dev/ESP32-fg/Fases/fase-03-sensores-actuadores.md` (950 lines — major update, expect ~500-600 lines after removal)

**Dependencies**: T-01

**Estimated effort**: 2 h

**Acceptance criteria**:
- `grep "DS18B20" Fases/fase-03-sensores-actuadores.md` → at least 1 match as primary example (AC-10)
- `grep -i "piscifactoria\|estanque\|acuicultura" Fases/fase-03-sensores-actuadores.md` → zero matches
- pH, DO, ORP sub-tasks are removed from the doc body
- Cross-reference to `examples/fish-farm/sensors.md` is present
- DS18B20 sub-task (3.1) is intact and complete

---

### [x] T-07 — Major update Tutorial/tutorial-03-sensores-actuadores.md

**Spec requirements**: DS18B20 as Universal Sensor Example (example-projects), Generic IIoT Framing (tutorial-narrative)

**Description**: Major update of the 540-line Tutorial 3 doc. Apply the same pedagogical restructuring as T-06: keep DS18B20 and generic sensor concepts as the core teaching material; remove inline fish-farm sensor theory (pH calibration, DO temperature compensation, ORP chemistry). Add a clearly labeled callout box pointing domain-specific learners to `examples/fish-farm/`. Replace all fish-farm narrative language. The actuator section and signal filtering section remain unchanged structurally (just narrative sweep).

**Files affected**:
- `/home/composedof2/Dev/ESP32-fg/Tutorial/tutorial-03-sensores-actuadores.md` (540 lines — major update, expect ~350-420 lines)

**Dependencies**: T-06 (tutorial must align with Fase doc)

**Estimated effort**: 2 h

**Acceptance criteria**:
- `grep -i "piscifactoria\|estanque\|acuicultura" Tutorial/tutorial-03-sensores-actuadores.md` → zero matches
- No inline pH, DO, ORP, turbidity theory in the body
- Cross-reference to `examples/fish-farm/` is present
- DS18B20 is the primary sensor example in tutorial prose

---

### [x] T-08 — Update docs/replanificacion/02-protocolo-unificado.md

**Spec requirements**: Configurable Namespace in Protocol Docs (configurable-mqtt-namespace), Sensor Type Enum — CUSTOM=0xFF (sensor-protocol)

**Description**: Moderate update to the 199-line protocol doc. Two targeted additions:

1. **sensor_type_t enum**: Add `SENSOR_TYPE_CUSTOM = 0xFF` after `SENSOR_TYPE_TURBIDITY = 0x06`. Add comment: "Tipo de sensor personalizado para sensores definidos por el usuario. La interpretación de values[] es específica de la aplicación." Document that `protocol_sensor_type_str()` returns `"CUSTOM"` for 0xFF. Confirm existing values (TEMP=0x01, PH=0x02, DO=0x03, ORP=0x04, LEVEL=0x05, TURBIDITY=0x06) are unchanged.

2. **MQTT namespace section**: Add new section "Espacio de Nombres MQTT" documenting: topic pattern `{mqtt_ns}/{gateway_id}/nodo/{nodo_id}/{tipo_sensor}`, NVS key `mqtt_namespace`, default value `iiot-kit`. Show example with `mqtt_ns = "greenhouse"`. Note that the old hardcoded `piscifactoria/` prefix is now an example namespace found in `examples/fish-farm/mqtt-topics.md`.

**Files affected**:
- `/home/composedof2/Dev/ESP32-fg/docs/replanificacion/02-protocolo-unificado.md` (199 lines — moderate update, ~15-25 lines added)

**Dependencies**: T-01 (vocabulary alignment)

**Estimated effort**: 1 h

**Acceptance criteria**:
- `grep "SENSOR_TYPE_CUSTOM" docs/replanificacion/02-protocolo-unificado.md` → match with value `0xFF` (AC-03)
- `grep "mqtt_ns\|iiot-kit" docs/replanificacion/02-protocolo-unificado.md` → matches for both (AC-04)
- `grep "0x01\|TEMPERATURE" docs/replanificacion/02-protocolo-unificado.md` → existing values still present (no regressions)
- Configurable namespace example shows `{mqtt_ns}/{gateway_id}/...` pattern

---

### [x] T-09 — Update knowledge/Proyecto/Arquitectura.md

**Spec requirements**: Project Identity Files Updated (tutorial-narrative), Generic IIoT Framing (tutorial-narrative)

**Description**: Moderate update. Replace the architecture topology diagram with the new autonomous gateway model (Preact SPA in SPIFFS, REST, WebSocket, optional MQTT). Remove the external server layer (Bun/Node, SQLite). Update decision links to reference the new ADR-004 once created (add a placeholder `<!-- will be created in T-13 -->` if needed). Update narrative prose to match generic IIoT framing.

**Files affected**:
- `/home/composedof2/Dev/ESP32-fg/knowledge/Proyecto/Arquitectura.md` (moderate update)

**Dependencies**: T-01, T-02

**Estimated effort**: 0.75 h

**Acceptance criteria**:
- `grep -i "piscifactoria\|bun\|sqlite" knowledge/Proyecto/Arquitectura.md` → zero matches
- Topology diagram shows Preact SPA, REST API, WebSocket, optional MQTT
- No external server layer shown as required component

---

### [x] T-10 — Update knowledge/Referencia/Protocolo-Mensajes.md

**Spec requirements**: Sensor Type Enum — CUSTOM=0xFF (sensor-protocol)

**Description**: Minor update. Add a reference to `SENSOR_TYPE_CUSTOM = 0xFF` in the sensor type enum table or section. This is a knowledge vault cross-reference file — keep the change minimal (1-5 lines). Add a note that 0xFF is the extension point for user-defined sensor types.

**Files affected**:
- `/home/composedof2/Dev/ESP32-fg/knowledge/Referencia/Protocolo-Mensajes.md` (minor update)

**Dependencies**: T-08 (must be consistent with protocol doc)

**Estimated effort**: 0.25 h

**Acceptance criteria**:
- `grep "SENSOR_TYPE_CUSTOM\|0xFF" knowledge/Referencia/Protocolo-Mensajes.md` → at least 1 match
- Existing enum values remain listed and unchanged

---

## Wave 4 — New Files (Parallelizable after Wave 3 completes)

All Wave 4 tasks can run in parallel with each other. They have no dependencies on each other.

---

### T-11 — Create examples/fish-farm/ directory and README.md

**Spec requirements**: Fish-Farm Example Directory (example-projects), DS18B20 as Universal Sensor Example (example-projects)

**Description**: Create the `examples/fish-farm/` directory structure. Write `examples/fish-farm/README.md` with: overview of the fish-farm use case, hardware requirements (aquaculture sensors), how to apply this example on top of the generic IIoT kit tutorial, a brief table of sensors covered (pH, DO, ORP, turbidity, level), reference to `sensors.md` and `mqtt-topics.md` files in this directory, and calibration overview note. Full fish-farm vocabulary is allowed here. Language: European Spanish (Spain).

**Files affected**:
- `/home/composedof2/Dev/ESP32-fg/examples/fish-farm/README.md` (new file, ~80-120 lines)

**Dependencies**: T-06 (README must be consistent with what Fase 3 cross-references)

**Estimated effort**: 1 h

**Acceptance criteria**:
- File exists at `examples/fish-farm/README.md`
- Contains overview, sensor table (pH, DO, ORP, turbidity, level), and links to sensors.md and mqtt-topics.md
- Cross-referenced from Fase 3 doc (T-06 will have added the callout)

---

### T-12 — Create examples/fish-farm/sensors.md

**Spec requirements**: Fish-Farm Example Directory (example-projects), DS18B20 as Universal Sensor Example (example-projects)

**Description**: Create `examples/fish-farm/sensors.md` containing the sensor content removed from Fase 3 in T-06. Transfer and expand: Sub-task 3.2 (pH driver + calibration procedure), Sub-task 3.3 (DO driver + temperature compensation), Sub-task 3.4 (ORP driver + ultrasonic level sensor). Add introduction noting these are domain-specific extensions of the generic sensor pattern taught in Fase 3. Preserve all technical detail (calibration coefficients, driver patterns, compensation formulas). Language: European Spanish (Spain).

**Files affected**:
- `/home/composedof2/Dev/ESP32-fg/examples/fish-farm/sensors.md` (new file, ~200-280 lines — content migrated from Fase 3)

**Dependencies**: T-06 (content comes from what was removed in T-06), T-11 (README must exist first for coherent directory)

**Estimated effort**: 1.5 h

**Acceptance criteria**:
- File exists at `examples/fish-farm/sensors.md`
- Contains pH, DO, ORP, and level sensor sections with calibration and driver details
- Each sensor section has at least: purpose, hardware connection, driver approach, calibration procedure
- Technical content is complete (not just headers)

---

### T-13 — Create examples/fish-farm/mqtt-topics.md

**Spec requirements**: Fish-Farm MQTT Topics in Example (configurable-mqtt-namespace), Configurable Namespace in Protocol Docs (configurable-mqtt-namespace)

**Description**: Create `examples/fish-farm/mqtt-topics.md` documenting the fish-farm-specific MQTT topic hierarchy. Use the configurable namespace pattern: `fish-farm/{gateway_id}/nodo/{nodo_id}/{tipo_sensor}`. Show the full topic tree for all fish-farm sensor types. Include a note explaining this uses `mqtt_ns = "fish-farm"` as the NVS configuration, referencing the generic namespace docs in `02-protocolo-unificado.md`. Language: European Spanish (Spain).

**Files affected**:
- `/home/composedof2/Dev/ESP32-fg/examples/fish-farm/mqtt-topics.md` (new file, ~60-100 lines)

**Dependencies**: T-08 (must reference the configurable namespace pattern defined there)

**Estimated effort**: 0.75 h

**Acceptance criteria**:
- File exists at `examples/fish-farm/mqtt-topics.md`
- Topic hierarchy uses `fish-farm/{gateway_id}/...` pattern (not hardcoded `piscifactoria/`)
- References `mqtt_ns = "fish-farm"` as the NVS configuration
- Topics for pH, DO, ORP, turbidity, level sensors are listed

---

### T-14 — Create knowledge/Decisiones/004-iiot-kit-transformation.md

**Spec requirements**: Repository Name Update (repo-rename) — ADR requirement

**Description**: Create the Architecture Decision Record for this transformation. Follow the template at `knowledge/Decisiones/_template.md`. Content: date (2026-05-23), decision statement (transform to generic IIoT platform), context (fish-farm framing limits audience; 85-95% of content is domain-agnostic), options considered (keep fish-farm framing, abstract fully, generic core + examples directory), decision (generic core + examples), rationale, consequences (repo rename needed, Fase 4 architecture changed), and references to related decisions (DC-01 to DC-13).

**Files affected**:
- `/home/composedof2/Dev/ESP32-fg/knowledge/Decisiones/004-iiot-kit-transformation.md` (new file, ~80-120 lines)

**Dependencies**: T-01, T-04 (ADR captures decisions already made in Waves 1-2)

**Estimated effort**: 0.75 h

**Acceptance criteria**:
- File exists at `knowledge/Decisiones/004-iiot-kit-transformation.md` (AC-06 partial)
- Contains: date, decision, context, options considered, rationale, consequences
- Follows structure of existing decision files in `knowledge/Decisiones/`

---

### T-15 — Create knowledge/Conceptos/Preact.md, REST-API.md, WebSocket.md

**Spec requirements**: Project Identity Files Updated — knowledge vault updated (tutorial-narrative)

**Description**: Create three concept notes for the Obsidian vault. Each note should follow the pattern of existing concept files (e.g., `knowledge/Conceptos/MQTT.md`). Content:

- **Preact.md**: What Preact is, why it matters for embedded (3KB vs React's 40KB+), how it differs from React, when to use it. Link to Fase 4.
- **REST-API.md**: REST principles on constrained devices, resource naming, HTTP status codes, cJSON in ESP-IDF, request/response patterns. Link to Fase 4.
- **WebSocket.md**: WebSocket protocol basics, bidirectional communication, contrast with polling and SSE, `esp_http_server` WebSocket support, reconnection patterns. Link to Fase 4.

Language: European Spanish (Spain). These are learning notes, not implementation specs.

**Files affected**:
- `/home/composedof2/Dev/ESP32-fg/knowledge/Conceptos/Preact.md` (new file, ~50-80 lines)
- `/home/composedof2/Dev/ESP32-fg/knowledge/Conceptos/REST-API.md` (new file, ~50-80 lines)
- `/home/composedof2/Dev/ESP32-fg/knowledge/Conceptos/WebSocket.md` (new file, ~50-80 lines)

**Dependencies**: T-04, T-05 (concept notes should align with what Fase 4 teaches)

**Estimated effort**: 1.5 h (three short files, can write sequentially in one pass)

**Acceptance criteria**:
- All 3 files exist (AC-06)
- Each file has substantive content (not just a title and stub)
- Each file links back to Fase 4 or Tutorial 4

---

## Wave 5 — Narrative Sweep + Site Config (Sequential after Wave 4)

Wave 5 tasks touch many files but each is a contained "find fish-farm language and replace" operation. Tasks T-16 and T-17 group the minor narrative files by category for efficiency. T-18, T-19, T-20, T-21 handle the remaining supporting docs.

---

### T-16 — Narrative sweep: all remaining Fases/ files (minor updates)

**Spec requirements**: Generic IIoT Framing Across All Docs (tutorial-narrative)

**Description**: Apply the replacement vocabulary table from the design doc to all remaining Fases files not already handled in Waves 2-3. For each file: grep for banned terms, replace with generic IIoT equivalents, verify no semantic changes (the technical content stays; only the narrative wrapper changes). Add domain-example callouts where appropriate ("Por ejemplo, en un sistema de monitorización de piscifactoría, ...").

Files to update (minor changes):
- `Fases/fase-00-preparacion.md` — Remove fish-farm language; add Preact/Vite tooling to "what you'll need" section
- `Fases/fase-01-gateway-nucleo.md` — Replace fish-farm references; WiFi/NVS/HTTP content is already generic
- `Fases/fase-02-nodos-espnow.md` — Replace fish-farm references; ESP-NOW content is already generic
- `Fases/fase-05-ota-optimizacion.md` — Replace fish-farm references; OTA/security is already generic
- `Fases/fase-06-mejoras-futuras.md` — Replace fish-farm references; update Sparkplug B context
- `Fases/README.md` — Update overview text for new project identity

**Files affected**:
- `/home/composedof2/Dev/ESP32-fg/Fases/fase-00-preparacion.md`
- `/home/composedof2/Dev/ESP32-fg/Fases/fase-01-gateway-nucleo.md`
- `/home/composedof2/Dev/ESP32-fg/Fases/fase-02-nodos-espnow.md`
- `/home/composedof2/Dev/ESP32-fg/Fases/fase-05-ota-optimizacion.md`
- `/home/composedof2/Dev/ESP32-fg/Fases/fase-06-mejoras-futuras.md`
- `/home/composedof2/Dev/ESP32-fg/Fases/README.md`

**Dependencies**: T-04, T-06 (anchor Fases must be done first to provide vocabulary reference)

**Estimated effort**: 2 h (six files, minor changes each)

**Acceptance criteria**:
- `grep -i "piscifactoria\|estanque\|acuicultura" Fases/fase-00-preparacion.md Fases/fase-01-gateway-nucleo.md Fases/fase-02-nodos-espnow.md Fases/fase-05-ota-optimizacion.md Fases/fase-06-mejoras-futuras.md Fases/README.md` → zero matches
- Preact/Vite tooling mention added to fase-00
- Technical content is unchanged (only narrative wrapper modified)

---

### T-17 — Narrative sweep: all remaining Tutorial/ files (minor updates)

**Spec requirements**: Generic IIoT Framing Across All Docs (tutorial-narrative)

**Description**: Apply the replacement vocabulary table to all remaining Tutorial files not already handled in Waves 2-3. Same pattern as T-16: grep, replace, verify semantics unchanged. Add domain-example callouts where appropriate.

Files to update (minor changes):
- `Tutorial/tutorial-00-preparacion.md` — Narrative update, add Preact/Vite tooling mention
- `Tutorial/tutorial-01-gateway-nucleo.md` — Replace fish-farm language; content is already generic
- `Tutorial/tutorial-02-nodos-espnow.md` — Replace fish-farm language; content is already generic
- `Tutorial/tutorial-05-ota-optimizacion.md` — Replace fish-farm language
- `Tutorial/tutorial-06-mejoras-futuras.md` — Replace fish-farm language, update future work section
- `Tutorial/README.md` — Update overview text for new project identity

**Files affected**:
- `/home/composedof2/Dev/ESP32-fg/Tutorial/tutorial-00-preparacion.md`
- `/home/composedof2/Dev/ESP32-fg/Tutorial/tutorial-01-gateway-nucleo.md`
- `/home/composedof2/Dev/ESP32-fg/Tutorial/tutorial-02-nodos-espnow.md`
- `/home/composedof2/Dev/ESP32-fg/Tutorial/tutorial-05-ota-optimizacion.md`
- `/home/composedof2/Dev/ESP32-fg/Tutorial/tutorial-06-mejoras-futuras.md`
- `/home/composedof2/Dev/ESP32-fg/Tutorial/README.md`

**Dependencies**: T-05, T-07 (anchor tutorials must be done first)

**Estimated effort**: 2 h (six files, minor changes each)

**Acceptance criteria**:
- `grep -i "piscifactoria\|estanque\|acuicultura" Tutorial/tutorial-00-preparacion.md Tutorial/tutorial-01-gateway-nucleo.md Tutorial/tutorial-02-nodos-espnow.md Tutorial/tutorial-05-ota-optimizacion.md Tutorial/tutorial-06-mejoras-futuras.md Tutorial/README.md` → zero matches
- Technical content is unchanged

---

### T-18 — Update mkdocs.yml and docs_site/index.md

**Spec requirements**: MkDocs Site Name and Navigation (repo-rename), Repository Name Update (repo-rename)

**Description**: Update `mkdocs.yml`:
- Set `site_name: ESP32-IIoT-Kit`
- Update `site_description` to generic IIoT platform description
- Update `repo_url` and `repo_name` to `ESP32-IIoT-Kit` (note: the GitHub rename is a separate step; use the new name here but be aware the URL won't resolve until the GitHub rename is done — add a comment)
- Rename Fase 4 nav entry to "Dashboard Embebido + API REST"
- Add `examples/` section to nav pointing to `examples/fish-farm/README.md`, `examples/fish-farm/sensors.md`, `examples/fish-farm/mqtt-topics.md`
- Update `site_url` to new GitHub Pages URL: `https://fgjcarlos.github.io/ESP32-IIoT-Kit/`

Update `docs_site/index.md`:
- Rewrite landing page to describe ESP32-IIoT-Kit as a generic IIoT platform tutorial
- Remove fish-farm-specific description
- Add brief mention of the examples directory

**Files affected**:
- `/home/composedof2/Dev/ESP32-fg/mkdocs.yml` (88 lines — moderate update)
- `/home/composedof2/Dev/ESP32-fg/docs_site/index.md` (moderate update)

**Dependencies**: T-11, T-12, T-13 (examples files must exist before adding them to nav)

**Estimated effort**: 1 h

**Acceptance criteria**:
- `grep "site_name" mkdocs.yml` → contains `ESP32-IIoT-Kit` (AC-07)
- `grep "Dashboard Embebido" mkdocs.yml` → at least 1 match in nav section
- examples/ section present in nav with all 3 fish-farm files
- `grep -i "piscifactoria\|estanque\|acuicultura" docs_site/index.md` → zero matches

---

### T-19 — Update README.md

**Spec requirements**: Project Identity Files Updated (tutorial-narrative), Repository Name Update (repo-rename)

**Description**: Major update of the 53-line README.md. Add: new project name and one-line description ("ESP32-IIoT-Kit — Plataforma IIoT genérica sobre ESP32"), the new architecture diagram (from proposal.md), a stack table (ESP32-S3 gateway, Preact SPA, REST API, WebSocket, ESP-NOW), quick start section, and `examples/fish-farm/` reference. Remove fish-farm-specific description. Note the pending GitHub rename (add badge placeholder). README should be welcoming to any IIoT use case, not just fish farming.

**Files affected**:
- `/home/composedof2/Dev/ESP32-fg/README.md` (53 lines — major update, expect ~120-180 lines)

**Dependencies**: T-01, T-02 (follow vocabulary and architecture from anchor files)

**Estimated effort**: 1 h

**Acceptance criteria**:
- `grep -i "piscifactoria\|estanque\|acuicultura" README.md` → zero matches
- Project name "ESP32-IIoT-Kit" present in title
- Architecture diagram present
- `examples/fish-farm/` referenced

---

### T-20 — Update docs/replanificacion/ supporting files

**Spec requirements**: Generic IIoT Framing Across All Docs (tutorial-narrative), Configurable Namespace in Protocol Docs (configurable-mqtt-namespace)

**Description**: Update two replanificacion docs:

- `docs/replanificacion/00-resumen.md`: Add a note documenting the IIoT transformation decision (date: 2026-05-23, change name: iiot-kit-transformation, scope summary). Keep existing replanning content intact.
- `docs/replanificacion/03-fases-corregidas.md`: Update the Fase 3 and Fase 4 summaries to reflect the new architecture. Fase 4 summary should reflect "Dashboard Embebido + API REST" title and scope.

**Files affected**:
- `/home/composedof2/Dev/ESP32-fg/docs/replanificacion/00-resumen.md` (moderate update)
- `/home/composedof2/Dev/ESP32-fg/docs/replanificacion/03-fases-corregidas.md` (moderate update)

**Dependencies**: T-04, T-06 (must reflect what was changed in those tasks)

**Estimated effort**: 0.75 h

**Acceptance criteria**:
- `grep -i "iiot-kit\|IIoT" docs/replanificacion/00-resumen.md` → at least 1 match
- Fase 4 entry in 03-fases-corregidas.md references "Dashboard Embebido + API REST"
- `grep -i "piscifactoria" docs/replanificacion/00-resumen.md docs/replanificacion/03-fases-corregidas.md` → zero matches (context-sensitive — historical references about the replanning are acceptable if clearly labeled as history)

---

### T-21 — Update remaining knowledge/ files

**Spec requirements**: Generic IIoT Framing Across All Docs (tutorial-narrative), Sensor Type Enum (sensor-protocol)

**Description**: Minor updates to three knowledge vault files:

- `knowledge/Proyecto/Progreso.md`: Update project status (Fase 0, replanificación completada 2026-05-23, iiot-kit-transformation in progress). Add iiot-kit-transformation to the change log section if one exists.
- `knowledge/Conceptos/MQTT.md`: Add a note: "En ESP32-IIoT-Kit, MQTT es una integración opcional, no el canal primario de datos. Ver Fase 4 para el diseño de dashboard embebido." Add note about configurable namespace (`mqtt_ns` in NVS).
- `knowledge/Referencia/Errores-Comunes.md`: Update or add entries that are now relevant to the new architecture (e.g., serving Preact from SPIFFS, WebSocket connection drops, REST endpoint errors). Remove or update any entries that assume the old external server architecture.

**Files affected**:
- `/home/composedof2/Dev/ESP32-fg/knowledge/Proyecto/Progreso.md` (minor update)
- `/home/composedof2/Dev/ESP32-fg/knowledge/Conceptos/MQTT.md` (minor update)
- `/home/composedof2/Dev/ESP32-fg/knowledge/Referencia/Errores-Comunes.md` (minor update)

**Dependencies**: T-04, T-08 (must be consistent with Fase 4 and protocol updates)

**Estimated effort**: 0.75 h

**Acceptance criteria**:
- `knowledge/Proyecto/Progreso.md` references iiot-kit-transformation
- `knowledge/Conceptos/MQTT.md` contains "opcional" / optional integration note
- `knowledge/Referencia/Errores-Comunes.md` has no entries that assume the old Bun/Node architecture as required

---

## Wave 6 — Verification (Sequential, last)

---

### T-22 — Verification: terminology grep + MkDocs build + link check

**Spec requirements**: All (this is the cross-cutting acceptance check)

**Description**: Run all automated acceptance criteria checks. Document results. If any check fails, identify the source file and fix before marking this task done.

**Checks to run**:

1. **AC-01** — No fish-farm terms in core docs:
   ```
   grep -rn "piscifactoria\|estanque\|acuicultura\|acuicola\|granja de peces\|tanque de peces" \
     Fases/ Tutorial/ docs/ project.md CLAUDE.md README.md mkdocs.yml docs_site/ knowledge/ \
     --include="*.md" --include="*.yml" \
     | grep -v "examples/fish-farm" | grep -v "openspec/"
   ```
   Expected: zero matches

2. **AC-02** — examples/fish-farm exists:
   ```
   ls examples/fish-farm/README.md examples/fish-farm/sensors.md examples/fish-farm/mqtt-topics.md
   ```
   Expected: all 3 files present

3. **AC-03** — SENSOR_TYPE_CUSTOM in protocol:
   ```
   grep "SENSOR_TYPE_CUSTOM" docs/replanificacion/02-protocolo-unificado.md
   ```
   Expected: match with `0xFF`

4. **AC-04** — Configurable MQTT namespace:
   ```
   grep "mqtt_ns\|iiot-kit" docs/replanificacion/02-protocolo-unificado.md
   ```
   Expected: matches for both terms

5. **AC-05** — MkDocs build:
   ```
   mkdocs build
   ```
   Expected: exit code 0, zero errors

6. **AC-06** — New knowledge files:
   ```
   ls knowledge/Decisiones/004-iiot-kit-transformation.md \
      knowledge/Conceptos/Preact.md knowledge/Conceptos/REST-API.md knowledge/Conceptos/WebSocket.md
   ```
   Expected: all 4 files present

7. **AC-07** — mkdocs.yml site name:
   ```
   grep "site_name" mkdocs.yml
   ```
   Expected: contains `ESP32-IIoT-Kit`

8. **AC-08** — CLAUDE.md no external server:
   ```
   grep -i "bun\|node\.js\|sqlite\|express" CLAUDE.md
   ```
   Expected: zero matches

9. **AC-09** — Fase 4 title:
   ```
   grep "Dashboard Embebido" Fases/fase-04-mqtt-dashboard.md
   ```
   Expected: at least 1 match

10. **AC-10** — DS18B20 in Fase 3:
    ```
    grep "DS18B20" Fases/fase-03-sensores-actuadores.md
    ```
    Expected: at least 1 match as primary example

**Files affected**: Read-only verification (may require targeted fixes in any file from prior tasks)

**Dependencies**: All Wave 1-5 tasks (T-01 through T-21) must be complete

**Estimated effort**: 1 h (including fix iterations for any failures)

**Acceptance criteria**:
- All 10 AC checks pass with zero failures
- MkDocs build produces zero errors
- Results are documented (pass/fail per check)

---

## Dependency Graph (Summary)

```
T-01 (project.md)
 ├── T-02 (CLAUDE.md)
 │    └── T-03 (AI_CONTEXT.md)
 ├── T-04 (fase-04) ──► T-05 (tutorial-04)
 │                  ──► T-14 (ADR-004)
 │                  ──► T-15 (concept notes)
 ├── T-06 (fase-03) ──► T-07 (tutorial-03)
 │                  ──► T-11 (examples/README)
 │                       └── T-12 (examples/sensors)
 ├── T-08 (protocolo) ──► T-10 (Protocolo-Mensajes)
 │                    ──► T-13 (fish-farm mqtt-topics)
 └── T-09 (Arquitectura.md)

[After T-04, T-06 done:]
 T-16 (Fases narrative sweep)
 T-17 (Tutorial narrative sweep)
 T-19 (README)
 T-20 (replanificacion docs)

[After T-11, T-12, T-13 done:]
 T-18 (mkdocs.yml + index.md)

[After T-04, T-08 done:]
 T-21 (knowledge vault minor)

[After ALL above:]
 T-22 (Verification)
```

---

## Parallel Execution Groups

| Group | Tasks | Can run in parallel |
|-------|-------|---------------------|
| Wave 1 | T-01 → T-02 → T-03 | Sequential only |
| Wave 2 | T-04 → T-05 | Sequential pair; parallel with Wave 3 |
| Wave 3a | T-06 → T-07 | Sequential pair; parallel with Wave 2 |
| Wave 3b | T-08 → T-10 | Sequential pair; parallel with Wave 2 and 3a |
| Wave 3c | T-09 | Standalone; parallel with Wave 2, 3a, 3b |
| Wave 4 | T-11 → T-12 (sequential), T-13, T-14, T-15 | T-13/T-14/T-15 parallel; T-12 after T-11 |
| Wave 5 | T-16, T-17, T-19 (parallel), T-18 (after T-11/12/13), T-20, T-21 | Mostly parallel |
| Wave 6 | T-22 | Sequential last |

---

## Review Workload Forecast

### Estimated Total Changed Lines

| Category | Files | Est. Lines Changed |
|----------|-------|-------------------|
| Full rewrites (project.md, fase-04, tutorial-04) | 3 | ~2200 (950+950+454 replaced with new content of similar length) |
| Major updates (CLAUDE.md, AI_CONTEXT.md, fase-03, tutorial-03, README) | 5 | ~900 (partial rewrites, removals, additions) |
| Moderate updates (02-protocolo, mkdocs.yml, index.md, 00-resumen, 03-fases, Arquitectura) | 6 | ~300 |
| Minor narrative sweep (12 files in Fases+Tutorial) | 12 | ~300 (targeted replacements) |
| Minor knowledge updates (Progreso, MQTT, Errores-Comunes, Protocolo-Mensajes) | 4 | ~60 |
| New files (3 examples, 4 knowledge) | 7 | ~700 |
| **Total** | **37** | **~4,460 lines** |

### 400-Line Budget Risk: HIGH

This is a documentation-only change but the total line count far exceeds 400 lines. The Fase 4 rewrite alone (T-04) touches ~950 lines.

### Chained PRs Recommended: Yes

**Suggested split** (respects wave dependency order):

- **PR 1 (Wave 1 + anchors)**: T-01, T-02, T-03 — ~400 lines. Establishes new vocabulary and architecture. Reviewable standalone.
- **PR 2 (Wave 2 + 3 core)**: T-04, T-05, T-06, T-07, T-08, T-09, T-10 — ~3200 lines. Largest batch; may need further splitting if the project team requires <400 line PRs. Candidate sub-split: T-04+T-05 as one PR, T-06+T-07 as a second, T-08+T-09+T-10 as a third.
- **PR 3 (Wave 4 — new files)**: T-11, T-12, T-13, T-14, T-15 — ~700 lines of new content. Clean addition with no deletions.
- **PR 4 (Wave 5 — sweep)**: T-16, T-17, T-18, T-19, T-20, T-21 — ~660 lines. Mechanical changes; lower review risk.
- **PR 5 (Wave 6 — verification + fixes)**: T-22 — verification run and any fix commits.

### Decision Needed Before Apply: Yes

The delivery strategy is `ask-on-risk`. With estimated ~4,460 changed lines across 37 files, the orchestrator MUST ask the user before launching `sdd-apply` whether to:

1. Proceed with a single PR (requires `size:exception` label or maintainer approval)
2. Split into the 5 PRs suggested above
3. Use a different split strategy

This decision must be captured before apply begins.
