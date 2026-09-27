
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

