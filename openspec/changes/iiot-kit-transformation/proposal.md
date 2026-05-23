# Proposal: ESP32-IIoT-Kit Transformation

## Intent

The project "Piscifactoria ESP32" is currently framed as a fish-farm monitoring system, but 85-95% of its technical content is domain-agnostic IIoT infrastructure (ESP-NOW, WiFi, OTA, deep sleep, NVS, FreeRTOS). The fish-farm framing limits the tutorial's audience and reusability.

**What**: Rebrand and restructure as "ESP32-IIoT-Kit" -- a generic IIoT platform tutorial with fish-farm as one example. Eliminate external server/dashboard dependencies and replace with an autonomous ESP32-S3 gateway running an embedded Preact SPA.

**Why now**: No firmware code exists yet (Fase 0). This is the cheapest possible moment to pivot -- only documentation, planning files, and learning notes need to change.

**Success**: A developer with any IIoT use case (agriculture, HVAC, manufacturing) can follow the tutorial. Fish-farm content lives in `examples/fish-farm/` for reference.

## Scope

### In Scope
- Repo rename: ESP32-fg -> ESP32-IIoT-Kit (GitHub)
- Rewrite project.md, CLAUDE.md, README.md, AI_CONTEXT.md for generic IIoT framing
- Full rewrite of Fase 4 / Tutorial 4 (embedded Preact + REST + WebSocket replaces Bun/React/MQTT)
- Major update of Fase 3 / Tutorial 3 (DS18B20 as universal example; fish-farm sensors to examples/)
- Narrative updates across all other Fases/Tutorials (remove fish-farm language, use generic IIoT)
- Update mkdocs.yml (site name, description, URLs, nav structure)
- Update docs_site/index.md landing page
- Add `examples/fish-farm/` directory with domain-specific sensor configs and calibration docs
- Add SENSOR_TYPE_CUSTOM = 0xFF to protocol docs (02-protocolo-unificado.md)
- Make MQTT topic namespace configurable (remove hardcoded `piscifactoria/` prefix)
- Update Obsidian knowledge vault (Arquitectura.md, Progreso.md, concept notes)
- New ADR: 004-iiot-kit-transformation.md
- Update GitHub Pages / MkDocs config

### Out of Scope
- Writing firmware code (none exists, none planned for this change)
- Changing the ESP-NOW protocol structure (header, ACK, etc. remain identical)
- Changing hardware choices (ESP32-S3 gateway, ESP32-C3 nodes)
- Implementing Preact SPA (that is Fase 4 implementation work)
- Sparkplug B changes (stays as Fase 6 future work)
- CI/CD pipeline changes beyond repo rename
- Translation to other languages (tutorial stays in European Spanish)

## Capabilities

### New Capabilities
- `embedded-dashboard`: Preact SPA served from ESP32-S3 SPIFFS, REST API via esp_http_server, WebSocket for real-time data push. Replaces external server + React dashboard.
- `example-projects`: Directory structure and convention for domain-specific implementations (fish-farm as first example).
- `configurable-mqtt-namespace`: MQTT topic prefix becomes a configurable parameter instead of hardcoded `piscifactoria/`.

### Modified Capabilities
- `sensor-protocol`: Add SENSOR_TYPE_CUSTOM = 0xFF to sensor_type_t enum; existing values remain as built-in examples.
- `tutorial-narrative`: All 7 tutorials reframed from fish-farm-specific to generic IIoT platform with domain examples.

## Approach

**Hybrid strategy**: Keep all technical content (protocol specs, ESP-IDF APIs, architecture decisions, learning concepts). Rewrite the narrative wrapper and Fase 4 architecture.

Three tiers of changes:

1. **Full rewrite** (3 files): project.md, fase-04, tutorial-04 -- these are architecturally different (no external server, embedded Preact instead of React, REST/WebSocket on ESP32 instead of Bun backend)
2. **Major update** (4 files): CLAUDE.md, fase-03, tutorial-03, README.md -- DS18B20 as universal example, structural changes to sensor examples
3. **Narrative sweep** (~20 files): Replace "piscifactoria", "estanque", "acuicultura" with generic IIoT language; add domain-example callouts. Mechanical find-and-replace with contextual review.

## Architecture (New)

```
                       ┌─────────────────────────────┐
                       │      ESP32-S3 GATEWAY        │
                       │      (Fully Autonomous)       │
                       │                               │
                       │  ┌─────────────────────────┐  │
   Browser ◄──────────►│  │  Preact SPA (SPIFFS)    │  │
   (any device)        │  │  ~200KB gzipped         │  │
                       │  └─────────────────────────┘  │
                       │  ┌─────────────────────────┐  │
   REST API ◄─────────►│  │  esp_http_server        │  │
                       │  │  Config + Data + OTA     │  │
                       │  └─────────────────────────┘  │
                       │  ┌─────────────────────────┐  │
   WebSocket ◄────────►│  │  Real-time data push    │  │
                       │  │  Sensor events stream    │  │
                       │  └─────────────────────────┘  │
                       │  ┌─────────────────────────┐  │
   MQTT (optional) ◄──►│  │  mqtt_bridge            │  │
                       │  │  Cloud/LAN integration   │  │
                       │  └─────────────────────────┘  │
                       │  ┌─────────────────────────┐  │
   ESP-NOW ◄──────────►│  │  espnow_manager         │  │
   (sensor nodes)      │  │  Backbone protocol       │  │
                       │  └─────────────────────────┘  │
                       └─────────────────────────────┘

         ESP-NOW (2.4GHz peer-to-peer)
              │
   ┌──────────┼──────────────┐
   │          │              │
Node A     Node B         Node C
ESP32-C3   ESP32-C3       ESP32-C3
(any sensor) (any sensor) (any sensor)
Deep Sleep + ESP-NOW + sensor_manager
```

**Key architectural change**: The gateway is fully self-contained. No external server, no external database, no external dashboard. A browser connects directly to the gateway's WiFi AP and loads the Preact SPA from SPIFFS. Data flows via WebSocket in real-time and REST for history/config. MQTT is available as an optional integration for cloud/LAN connectivity but is not required for basic operation.

## File Change Matrix

| File | Action | Effort | Notes |
|------|--------|--------|-------|
| **Full Rewrites** | | | |
| `project.md` | Rewrite | High | New architecture, remove server/dashboard, add Preact, generic framing |
| `Fases/fase-04-mqtt-dashboard.md` | Rewrite | High | Becomes "Fase 4: Dashboard Embebido + API REST". Preact + WebSocket on ESP32, MQTT optional |
| `Tutorial/tutorial-04-mqtt-dashboard.md` | Rewrite | High | Teach Preact, REST API design, WebSocket on ESP32. MQTT section becomes optional chapter |
| **Major Updates** | | | |
| `CLAUDE.md` | Major | Medium | New architecture diagram, remove server/dashboard refs, add Preact/REST/WS APIs, update conventions |
| `AI_CONTEXT.md` | Major | Medium | Mirror CLAUDE.md changes for AI context |
| `README.md` | Major | Medium | New project name, architecture diagram, stack table |
| `Fases/fase-03-sensores-actuadores.md` | Major | Medium | DS18B20 as universal example, fish-farm sensors referenced as examples |
| `Tutorial/tutorial-03-sensores-actuadores.md` | Major | Medium | Same pedagogical restructuring |
| **Moderate Updates** | | | |
| `mkdocs.yml` | Moderate | Low | Site name, description, URLs, possibly nav restructure |
| `docs_site/index.md` | Moderate | Low | Landing page rewrite |
| `docs/replanificacion/00-resumen.md` | Moderate | Low | Add note about IIoT transformation |
| `docs/replanificacion/02-protocolo-unificado.md` | Moderate | Low | Add SENSOR_TYPE_CUSTOM, configurable MQTT namespace |
| `docs/replanificacion/03-fases-corregidas.md` | Moderate | Low | Update Fase 3-4 summaries |
| `knowledge/Proyecto/Arquitectura.md` | Moderate | Low | New architecture description |
| **Minor Narrative Updates** | | | |
| `Fases/fase-00-preparacion.md` | Minor | Low | Remove fish-farm language |
| `Fases/fase-01-gateway-nucleo.md` | Minor | Low | Remove fish-farm language |
| `Fases/fase-02-nodos-espnow.md` | Minor | Low | Remove fish-farm language |
| `Fases/fase-05-ota-optimizacion.md` | Minor | Low | Remove fish-farm language |
| `Fases/fase-06-mejoras-futuras.md` | Minor | Low | Remove fish-farm language, update Sparkplug B context |
| `Fases/README.md` | Minor | Low | Update overview |
| `Tutorial/README.md` | Minor | Low | Update overview |
| `Tutorial/tutorial-00-preparacion.md` | Minor | Low | Narrative update |
| `Tutorial/tutorial-01-gateway-nucleo.md` | Minor | Low | Narrative update |
| `Tutorial/tutorial-02-nodos-espnow.md` | Minor | Low | Narrative update |
| `Tutorial/tutorial-05-ota-optimizacion.md` | Minor | Low | Narrative update |
| `Tutorial/tutorial-06-mejoras-futuras.md` | Minor | Low | Narrative update |
| `knowledge/Proyecto/Progreso.md` | Minor | Low | Update status |
| `knowledge/Conceptos/MQTT.md` | Minor | Low | Add note about optional integration |
| `knowledge/Referencia/Protocolo-Mensajes.md` | Minor | Low | Add SENSOR_TYPE_CUSTOM |
| `knowledge/Referencia/Errores-Comunes.md` | Minor | Low | Update for new architecture |
| **New Files** | | | |
| `examples/fish-farm/README.md` | New | Medium | Fish-farm example overview, sensor configs, calibration |
| `examples/fish-farm/sensors.md` | New | Medium | pH, DO, ORP, turbidity sensor details moved here |
| `examples/fish-farm/mqtt-topics.md` | New | Low | Fish-farm-specific MQTT topic hierarchy |
| `knowledge/Decisiones/004-iiot-kit-transformation.md` | New | Low | ADR for the transformation decision |
| `knowledge/Conceptos/Preact.md` | New | Low | Preact concept note for Obsidian vault |
| `knowledge/Conceptos/REST-API.md` | New | Low | REST API on embedded concept note |
| `knowledge/Conceptos/WebSocket.md` | New | Low | WebSocket concept note |
| **Deleted** | | | |
| `server/` (directory) | Delete reference | None | Never existed as files; remove from docs/plans |
| `dashboard/` (directory) | Delete reference | None | Never existed as files; remove from docs/plans |

## Phase Adjustments Summary

**Fase 0 (Preparacion)**: Minor narrative changes. Replace fish-farm context with generic IIoT framing. Learning objectives and tool setup remain identical. Add Preact/Vite tooling to the "what you'll need" section.

**Fase 1 (Gateway Nucleo)**: Minor narrative changes. WiFi AP/STA, NVS, HTTP server, FreeRTOS task architecture are all domain-agnostic already. Replace "piscifactoria" references with generic device/sensor language.

**Fase 2 (Nodos + ESP-NOW)**: Minor narrative changes. ESP-NOW protocol, deep sleep, node firmware are fully generic. Update example scenarios from fish tanks to generic sensor deployments.

**Fase 3 (Sensores + Actuadores)**: Major update. DS18B20 (temperature) becomes the universal worked example that every student implements. Fish-farm-specific sensors (pH, DO, ORP, turbidity) move to `examples/fish-farm/` as advanced exercises. Add SENSOR_TYPE_CUSTOM = 0xFF to protocol documentation. Actuator control remains generic (relay/GPIO).

**Fase 4 (Dashboard + API)**: Full rewrite. Eliminates Bun/Node backend, SQLite, Mosquitto broker, and Vite+React dashboard. Replaces with: (1) Preact SPA embedded in SPIFFS, (2) REST API via esp_http_server for config/data/history, (3) WebSocket on ESP32 for real-time sensor data push. MQTT becomes an optional integration section rather than the primary data channel. This is the largest single change.

**Fase 5 (OTA + Seguridad)**: Minor narrative changes. OTA, NVS encryption, watchdog, security hardening are fully generic. Update references to match new architecture.

**Fase 6 (Mejoras Futuras)**: Minor updates. Sparkplug B migration stays as future work. MQTT is already optional by Fase 4. Add notes about cloud integration patterns as an advanced topic. Update references.

## New Directory Structure

```
ESP32-IIoT-Kit/                     # Renamed from ESP32-fg
├── firmware/                        # (unchanged - no code exists yet)
│   ├── gateway/
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
│   │   │   └── protocol/
│   │   ├── web/                       # Preact SPA source (built → SPIFFS)
│   │   │   ├── src/
│   │   │   │   ├── app.jsx
│   │   │   │   ├── components/
│   │   │   │   └── hooks/
│   │   │   ├── package.json
│   │   │   └── vite.config.js         # Preact + gzip build
│   │   ├── partitions.csv
│   │   └── CMakeLists.txt
│   ├── node/                          # (unchanged)
│   └── common/                        # (unchanged)
│       └── protocol/
│           ├── protocol.h             # + SENSOR_TYPE_CUSTOM = 0xFF
│           └── protocol.c
├── examples/                          # NEW: domain-specific examples
│   └── fish-farm/
│       ├── README.md
│       ├── sensors.md
│       └── mqtt-topics.md
├── Tutorial/                          # Narrative updated
├── Fases/                             # Narrative updated (Fase 4 rewritten)
├── knowledge/                         # Updated Obsidian vault
│   ├── Conceptos/
│   │   ├── Preact.md                  # NEW
│   │   ├── REST-API.md                # NEW
│   │   ├── WebSocket.md              # NEW
│   │   └── ... (existing, minor updates)
│   └── Decisiones/
│       └── 004-iiot-kit-transformation.md  # NEW
├── docs/                              # Updated
├── docs_site/                         # Updated
├── project.md                         # Rewritten
├── CLAUDE.md                          # Major update
├── AI_CONTEXT.md                      # Major update
├── README.md                          # Major update
├── mkdocs.yml                         # Updated
└── openspec/                          # SDD artifacts (this change)
```

## Protocol Changes

1. **sensor_type_t**: Add `SENSOR_TYPE_CUSTOM = 0xFF` to the enum. Existing values (TEMP, PH, DO, ORP, TURBIDITY, LEVEL) remain as built-in examples. Custom allows any user-defined sensor type without modifying the protocol.

2. **MQTT topic namespace**: Change from hardcoded `piscifactoria/{gateway_id}/...` to configurable `{namespace}/{gateway_id}/...` where namespace is stored in NVS and defaults to `iiot-kit`. The topic hierarchy structure remains identical.

3. **No protocol wire format changes**: The ESP-NOW header (13 bytes), payload structure, ACK mechanism, retry logic, and discovery flow are all unchanged.

## Risks

| Risk | Likelihood | Impact | Mitigation |
|------|------------|--------|------------|
| Fase 4 rewrite scope creep -- embedded dashboard design decisions bleed into implementation details | High | Medium | Keep Fase 4 docs at architectural level. Defer Preact component design, REST endpoint specs, and WebSocket message formats to implementation phases. |
| Inconsistent terminology after narrative sweep -- some files still say "piscifactoria" or assume fish-farm context | Medium | Low | Use automated grep to verify zero remaining fish-farm-specific language in core docs (excluding examples/). Add to success criteria. |
| SPIFFS size constraint for Preact SPA not validated -- 1.9MB may be tight with future features | Medium | Medium | Document the constraint clearly in Fase 4. Preact + Vite with gzip should produce ~50-150KB. Include size budget in docs. |
| MkDocs site breaks after repo rename -- GitHub Pages URL changes, internal links may break | Low | Medium | Update all URL references in mkdocs.yml. Set up redirect from old URL if possible. |
| Knowledge vault links break -- Obsidian wiki-links reference old file names | Low | Low | Wiki-links use file names not paths. Only new files are added, no renames of existing concept notes. |

## Rollback Plan

This is a documentation-only change with no code. Rollback is straightforward:
1. `git revert` the commit(s) that apply these changes
2. Rename repo back to ESP32-fg on GitHub if rename was applied
3. Re-enable old GitHub Pages URL

Since all changes are tracked in git and no firmware/runtime behavior is affected, rollback risk is minimal.

## Dependencies

- GitHub repo rename API or manual rename (ESP32-fg -> ESP32-IIoT-Kit)
- No external library or tooling dependencies for the documentation changes
- Preact/Vite tooling knowledge (for writing accurate Fase 4 docs) -- not needed for this change, but for future implementation

## Success Criteria

- [ ] Zero occurrences of "piscifactoria", "estanque", "acuicultura" in core docs (grep verification), except within `examples/fish-farm/`
- [ ] project.md describes a generic IIoT platform with autonomous ESP32-S3 gateway
- [ ] CLAUDE.md architecture diagram shows embedded Preact + REST + WebSocket, no external server
- [ ] Fase 4 docs describe embedded dashboard architecture (Preact + REST + WebSocket), MQTT as optional
- [ ] Fase 3 docs use DS18B20 as universal example, fish-farm sensors in examples/
- [ ] `examples/fish-farm/` directory exists with sensor configs and domain-specific content
- [ ] Protocol docs include SENSOR_TYPE_CUSTOM = 0xFF
- [ ] MQTT topic docs use configurable namespace
- [ ] mkdocs.yml reflects new project name and structure
- [ ] MkDocs site builds without errors
- [ ] All internal links resolve (no broken references)
- [ ] ADR 004 documents the transformation decision and rationale

## Estimated Effort

| Category | File Count | Effort |
|----------|-----------|--------|
| Full rewrites | 3 | ~6-8 hours |
| Major updates | 5 | ~4-5 hours |
| Moderate updates | 6 | ~2-3 hours |
| Minor narrative | 14 | ~3-4 hours |
| New files | 7 | ~3-4 hours |
| Verification + links | - | ~1-2 hours |
| **Total** | **~35 files** | **~19-26 hours** |
