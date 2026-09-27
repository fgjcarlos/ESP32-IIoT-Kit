
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

