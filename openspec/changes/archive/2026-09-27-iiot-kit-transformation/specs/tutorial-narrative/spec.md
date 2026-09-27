
# Delta for Tutorial Narrative


### Requirement: Generic IIoT Framing Across All Docs

All 7 Fases (`fase-00` through `fase-06`) and all 7 Tutorials (`tutorial-00` through `tutorial-06`) MUST use generic IIoT language. The terms "piscifactoria", "estanque", "acuicultura", and "pez/peces" MUST NOT appear outside `examples/fish-farm/`. Domain-specific content MUST use "sensor node", "IIoT gateway", "deployment", or equivalent neutral terms.


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


#### Scenario: New contributor reads CLAUDE.md

- GIVEN a new contributor opens `CLAUDE.md`
- WHEN they read the Architecture Summary section
- THEN the diagram shows Preact SPA in SPIFFS, REST API, and WebSocket on the ESP32-S3
- AND no external server, SQLite, or Bun/Node is shown as a required component

---

