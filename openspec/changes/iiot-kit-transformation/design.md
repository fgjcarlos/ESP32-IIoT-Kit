# Design: ESP32-IIoT-Kit Transformation

## Technical Approach

Documentation-only restructuring that transforms "Piscifactoria ESP32" into "ESP32-IIoT-Kit", a generic IIoT platform tutorial. The approach has three layers: (1) anchor files that define the new architecture are rewritten first, (2) dependent files are updated in dependency order to maintain consistency, (3) a final narrative sweep catches remaining domain-specific language. The Fase 4 rewrite is the highest-impact change, replacing the external server/dashboard architecture with an autonomous embedded Preact SPA + REST API + WebSocket design.

## Architecture Decisions

### Decision: Embedded-First Gateway (Fase 4 Architecture)

| Option | Tradeoff | Verdict |
|--------|----------|---------|
| Keep external server (Bun/Node + React) | Familiar stack, but requires 3+ running components; not autonomous | Rejected |
| Embedded Preact SPA + REST + WebSocket on ESP32 | Single autonomous device, offline-capable; limited by flash/RAM | **Chosen** |
| Embedded vanilla HTML/JS (current embedded dashboard) | Minimal footprint but poor maintainability for a full dashboard | Rejected |

**Rationale**: An IIoT kit must work standalone without external infrastructure. Preact (~3KB gzipped) with Vite produces builds under 200KB, well within the 1.9MB SPIFFS budget. The ESP32-S3's `esp_http_server` already supports WebSocket natively. This eliminates Mosquitto, Bun/Node, SQLite, and React as hard dependencies, turning them into optional cloud integrations. For an educational project, teaching embedded-first is more valuable than teaching a multi-service architecture.

### Decision: Content Layering Strategy

| Option | Tradeoff | Verdict |
|--------|----------|---------|
| Inline fish-farm examples throughout tutorial | Simple but locks readers into one domain | Rejected |
| Fully abstract (no domain examples at all) | Generic but dry; hard to follow without concrete context | Rejected |
| Generic core + `examples/` directory for domains | Teaches platform concepts with brief domain callouts; extensible | **Chosen** |

**Rationale**: The core tutorial (Fases 0-6) teaches the platform using DS18B20 (temperature) as the universal sensor. Domain-specific sensors (pH, DO, ORP, turbidity) move to `examples/fish-farm/`. Other domains (greenhouse, factory) can add their own `examples/` directories following the same pattern. Core docs reference examples with phrasing like "for a fish-farm deployment, see examples/fish-farm/" but never depend on them.

### Decision: File Processing Order

| Option | Tradeoff | Verdict |
|--------|----------|---------|
| Alphabetical / random order | Simple to parallelize but creates temporary inconsistencies | Rejected |
| Top-down (anchors first, dependents after) | Anchors define terminology and architecture; dependents follow naturally | **Chosen** |

**Rationale**: Three anchor files (project.md, CLAUDE.md, Fase 4 docs) define the new vocabulary, architecture diagrams, and API contracts. All other files reference these. Processing anchors first prevents inconsistencies where a minor-update file still references eliminated components.

### Decision: Repo Rename Timing

| Option | Tradeoff | Verdict |
|--------|----------|---------|
| Rename before doc changes | Clean URLs in docs, but breaks CI/Pages during transition | Rejected |
| Rename after all doc changes land | Content ready for new name; single rename commit at the end | **Chosen** |
| Never rename (keep ESP32-fg) | No disruption but contradicts project identity | Rejected |

**Rationale**: Rename last so that GitHub Pages continues working during the documentation rewrite. The final commit updates mkdocs.yml URLs, README badges, and performs the GitHub repo rename. This is a single atomic operation.

### Decision: Sensor Type Enum Strategy

| Option | Tradeoff | Verdict |
|--------|----------|---------|
| Remove all fish-farm types, only keep TEMPERATURE | Breaking change conceptually; loses pedagogical value | Rejected |
| Keep all existing types + add SENSOR_TYPE_CUSTOM = 0xFF | Backward compatible; existing types serve as examples; custom allows any domain | **Chosen** |

**Rationale**: Existing enum values (TEMP, PH, DO, ORP, LEVEL, TURBIDITY) remain as "built-in" sensor types that illustrate common use cases. The new `SENSOR_TYPE_CUSTOM = 0xFF` allows any user-defined sensor without modifying the protocol. In the core tutorial, only `SENSOR_TYPE_TEMPERATURE` is used. Other types are referenced in `examples/fish-farm/`.

## Data Flow

The new architecture simplifies data flow from a 4-component chain to a single-device loop:

```
CURRENT (eliminated):
  Sensor Node --> ESP-NOW --> Gateway --> MQTT --> Mosquitto --> Bun/Node
                                                                   |
                                                              SQLite + WS
                                                                   |
                                                              React Dashboard

NEW (Fase 4 rewrite):
  Sensor Node --> ESP-NOW --> Gateway (ESP32-S3)
                                 |
                        +--------+--------+
                        |        |        |
                     SPIFFS   REST API  WebSocket
                     (Preact   (config,  (real-time
                      SPA)     data,     sensor
                               OTA)     events)
                        |        |        |
                        +--------+--------+
                                 |
                            Browser (any device on WiFi AP)
                                 |
                     [Optional: MQTT bridge to cloud/LAN]
```

## File Changes

### Processing Order (5 waves)

**Wave 1 -- Anchors (do first, sequential)**

| File | Action | Description |
|------|--------|-------------|
| `project.md` | Rewrite | New architecture (embedded-first), remove server/dashboard stacks, generic IIoT framing. Defines vocabulary for all other files. |
| `CLAUDE.md` | Major update | New architecture diagram, update stack tables, remove server/dashboard refs, add Preact/REST/WS APIs, update conventions |
| `AI_CONTEXT.md` | Major update | Mirror CLAUDE.md changes |

**Wave 2 -- Fase 4 (full rewrite, can start after Wave 1)**

| File | Action | Description |
|------|--------|-------------|
| `Fases/fase-04-mqtt-dashboard.md` | Rewrite | Becomes "Fase 4: Dashboard Embebido + API REST". See Fase 4 Redesign section below. |
| `Tutorial/tutorial-04-mqtt-dashboard.md` | Rewrite | Teach embedded dashboard concepts: Preact on microcontrollers, REST API design, WebSocket for real-time data, MQTT as optional integration. |

**Wave 3 -- Fase 3 + Protocol + Structure (can start after Wave 1)**

| File | Action | Description |
|------|--------|-------------|
| `Fases/fase-03-sensores-actuadores.md` | Major update | DS18B20 as core example; pH/DO/ORP/ultrasonic tasks move to `examples/fish-farm/sensors.md`; keep actuator/filter/validation tasks generic |
| `Tutorial/tutorial-03-sensores-actuadores.md` | Major update | Same split; intro stays generic, fish-farm sensor theory to examples/ |
| `docs/replanificacion/02-protocolo-unificado.md` | Moderate | Add `SENSOR_TYPE_CUSTOM = 0xFF` to enum; add note about configurable MQTT namespace |
| `knowledge/Proyecto/Arquitectura.md` | Moderate | New topology diagram, remove server layer, update decision links |
| `knowledge/Referencia/Protocolo-Mensajes.md` | Minor | Add SENSOR_TYPE_CUSTOM reference |

**Wave 4 -- New files + supporting docs (can parallelize)**

| File | Action | Description |
|------|--------|-------------|
| `examples/fish-farm/README.md` | Create | Overview: how to apply fish-farm domain to the kit |
| `examples/fish-farm/sensors.md` | Create | pH, DO, ORP, turbidity, level -- sensor configs, calibration, driver details moved from Fase 3 |
| `examples/fish-farm/mqtt-topics.md` | Create | Fish-farm MQTT topic hierarchy with `piscifactoria/` namespace |
| `knowledge/Decisiones/004-iiot-kit-transformation.md` | Create | ADR documenting the transformation |
| `knowledge/Conceptos/Preact.md` | Create | Preact concept note |
| `knowledge/Conceptos/REST-API.md` | Create | REST on embedded concept note |
| `knowledge/Conceptos/WebSocket.md` | Create | WebSocket concept note |

**Wave 5 -- Narrative sweep (all remaining files, parallelizable)**

| File | Action | Description |
|------|--------|-------------|
| `README.md` | Major | New name, architecture diagram, stack table |
| `mkdocs.yml` | Moderate | site_name, site_description, repo_url, nav entries (add examples section, rename Fase 4 nav entry) |
| `docs_site/index.md` | Moderate | Landing page rewrite |
| `docs/replanificacion/00-resumen.md` | Moderate | Add IIoT transformation note |
| `docs/replanificacion/03-fases-corregidas.md` | Moderate | Update Fase 3-4 summaries |
| `Fases/fase-00-preparacion.md` | Minor | Replace fish-farm language |
| `Fases/fase-01-gateway-nucleo.md` | Minor | Replace fish-farm language |
| `Fases/fase-02-nodos-espnow.md` | Minor | Replace fish-farm language |
| `Fases/fase-05-ota-optimizacion.md` | Minor | Replace fish-farm language |
| `Fases/fase-06-mejoras-futuras.md` | Minor | Replace fish-farm language, update Sparkplug B context |
| `Fases/README.md` | Minor | Update overview |
| `Tutorial/README.md` | Minor | Update overview |
| `Tutorial/tutorial-00-preparacion.md` | Minor | Narrative update, add Preact/Vite tooling mention |
| `Tutorial/tutorial-01-gateway-nucleo.md` | Minor | Narrative update |
| `Tutorial/tutorial-02-nodos-espnow.md` | Minor | Narrative update |
| `Tutorial/tutorial-05-ota-optimizacion.md` | Minor | Narrative update |
| `Tutorial/tutorial-06-mejoras-futuras.md` | Minor | Narrative update |
| `knowledge/Proyecto/Progreso.md` | Minor | Update status |
| `knowledge/Conceptos/MQTT.md` | Minor | Add note: optional integration, not primary channel |
| `knowledge/Referencia/Errores-Comunes.md` | Minor | Update for new architecture |

**Post-waves (last)**

| File | Action | Description |
|------|--------|-------------|
| `mkdocs.yml` | Final pass | Update repo_url/site_url after GitHub rename |
| GitHub rename | Infra | ESP32-fg -> ESP32-IIoT-Kit |

## Fase 4 Redesign (Detailed)

The current Fase 4 has 4 sub-tasks with 14 tasks across 951 lines. The entire content is eliminated and replaced.

### New Fase 4 Structure

**Title**: "Fase 4: Dashboard Embebido + API REST"

**Sub-task 4.1: REST API on ESP32** (3 tasks)
- T4.1.1: Design and implement REST endpoint categories using `esp_http_server`: `/api/status`, `/api/nodes`, `/api/nodes/{id}/readings`, `/api/config`, `/api/actuators`
- T4.1.2: JSON serialization with cJSON for API responses
- T4.1.3: WebSocket handler on `esp_http_server` for real-time sensor data push

**Sub-task 4.2: Embedded Preact SPA** (3 tasks)
- T4.2.1: Scaffold Preact + Vite project in `firmware/gateway/web/`; configure Vite for gzip output targeting SPIFFS
- T4.2.2: Build SPA pages: dashboard (real-time node cards), config (WiFi, thresholds), status (gateway health)
- T4.2.3: Integrate SPIFFS serving; CMake build hook that builds Preact then flashes to SPIFFS partition

**Sub-task 4.3: MQTT as Optional Integration** (2 tasks)
- T4.3.1: Refactor `mqtt_bridge` docs as optional module; configurable namespace (`{namespace}/{gateway_id}/...` stored in NVS, default `iiot-kit`)
- T4.3.2: Document MQTT topics with generic namespace; fish-farm topics in `examples/fish-farm/mqtt-topics.md`

**Sub-task 4.4: Integration and Verification** (2 tasks)
- T4.4.1: End-to-end flow: node sensor -> ESP-NOW -> gateway -> REST/WebSocket -> browser
- T4.4.2: Size budget verification: Preact SPA must be under 500KB gzipped (1.9MB SPIFFS has room for OTA assets too)

### New Tutorial 4 Structure

**Concepts taught**:
1. Why embedded dashboards beat external servers for IIoT (autonomy, offline, single-device deployment)
2. REST API design principles on constrained devices (resource naming, status codes, cJSON)
3. WebSocket: bidirectional real-time communication (vs polling, vs SSE)
4. Preact: React-like development at 3KB (why not React, why not vanilla JS)
5. SPIFFS: serving static assets from flash (partition layout, size budgets, gzip)
6. MQTT as optional cloud integration (publish/subscribe review, configurable namespace)

## Fase 3 Restructuring

### What stays in core tutorial
- Sub-task 3.1 (DS18B20 driver + integration): KEEP entirely -- universal sensor example
- Sub-task 3.5 (validation + filtering): KEEP -- domain-agnostic signal processing
- Sub-task 3.6 (actuators + alerts): KEEP -- generic relay control, threshold alerts with hysteresis
- T3.1.2 (integration with node cycle): KEEP -- demonstrates sensor_manager pattern

### What moves to `examples/fish-farm/sensors.md`
- Sub-task 3.2 (pH driver + calibration): all 3 tasks
- Sub-task 3.3 (DO driver + temperature compensation): both tasks
- Sub-task 3.4 (ORP + ultrasonic level): both tasks

### How the tutorial references examples
Core Fase 3 docs end each sub-task with: "To implement domain-specific sensors such as pH, dissolved oxygen, or water level, see the worked examples in `examples/fish-farm/sensors.md`."

## Narrative Strategy

### Banned terms in core docs
`piscifactoria`, `piscifactorias`, `estanque`, `estanques`, `acuicultura`, `acuicola`, `acuicolas`, `granja de peces`, `tanque de peces`

### Replacement vocabulary
| Old (fish-farm) | New (generic IIoT) |
|-----------------|-------------------|
| piscifactoria | instalacion / planta / sistema |
| estanque | zona de monitoreo / punto de medida |
| acuicola | industrial / de proceso |
| sensor de temperatura del agua | sensor de temperatura |
| operador de la piscifactoria | operador / usuario |

### Example callout pattern (used in core docs)
> "Por ejemplo, en un sistema de monitorizacion de piscifactoria, este sensor mediria la temperatura del agua. Ver `examples/fish-farm/` para la configuracion completa."

### Language in `examples/fish-farm/`
Full fish-farm vocabulary is allowed and encouraged. The domain-specific language makes the example concrete and useful.

## Protocol Documentation Updates

In `docs/replanificacion/02-protocolo-unificado.md`:

1. **sensor_type_t enum**: Add `SENSOR_TYPE_CUSTOM = 0xFF` after `SENSOR_TYPE_TURBIDITY = 0x06`. Add comment: "Custom sensor type for user-defined sensors. Interpretation of values[] is application-specific."

2. **MQTT namespace**: Add a new section "MQTT Topic Namespace" documenting: `{namespace}/{gateway_id}/nodo/{nodo_id}/{tipo_sensor}` where `namespace` is configurable via NVS (key `"mqtt_namespace"`, default `"iiot-kit"`). Note that `piscifactoria/` is an example namespace used in `examples/fish-farm/mqtt-topics.md`.

3. **No wire format changes**: Header, payloads, ACK, discovery, config, OTA_PREPARE structs remain byte-identical.

## Testing Strategy

| Layer | What to Test | Approach |
|-------|-------------|----------|
| Link integrity | All internal markdown links resolve | `mkdocs build --strict` (after removing roamlinks strict mode issue) |
| Terminology | Zero banned terms in core docs | `grep -rn "piscifactoria\|estanque\|acuicola" --include="*.md"` excluding `examples/` and `openspec/` |
| Build | MkDocs site builds without errors | `mkdocs build` |
| Nav | All nav entries in mkdocs.yml point to existing files | `mkdocs build --strict` |

## Migration / Rollout

No migration required. All changes are documentation-only. Rollback is `git revert`. GitHub repo rename is the only non-git operation and can be reversed via GitHub settings.

## Open Questions

- [ ] Should `mkdocs.yml` nav add an "Examples" top-level section, or keep examples/ outside the MkDocs site (linked via README only)?
- [ ] Should the repo rename happen in the same PR as the doc changes, or as a separate follow-up step?
