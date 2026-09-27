
# Delta for Sensor Protocol


### Requirement: Sensor Type Enum

The `sensor_type_t` enum documented in `docs/replanificacion/02-protocolo-unificado.md` MUST include `SENSOR_TYPE_CUSTOM = 0xFF` as an escape hatch for user-defined sensor types. All existing values (TEMPERATURE=0x01, PH=0x02, DO=0x03, ORP=0x04, LEVEL=0x05, TURBIDITY=0x06) MUST be preserved unchanged. The `protocol_sensor_type_str()` function docs MUST specify that it returns `"CUSTOM"` for 0xFF.


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

