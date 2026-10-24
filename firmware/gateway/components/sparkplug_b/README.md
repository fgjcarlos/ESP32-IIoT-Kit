# sparkplug_b

Component for the gateway's optional Sparkplug B v3.0.0 mode (issue #11).

**Status:** implemented (commits 1-12 of `odd/tasks/issue-11-sparkplug-b/`).

When `CONFIG_SPARKPLUG_B_ENABLED=y`, the gateway acts as a Sparkplug B Edge
of Network (EoN) Node. Each sensor node known via ESP-NOW is represented as
a Device on the broker. Birth / Data / Death certificates, LWT, and the seq /
bdSeq state machine are owned by this component.

When the flag is off (default), this component compiles to an empty object and
`mqtt_bridge_start()` stays a no-op (Fase 4 placeholder).

## Modules

| File | Responsibility |
|---|---|
| `src/sparkplug_b_payload.c` | Protobuf codec over the official Tahu schema (nanopb) |
| `src/sparkplug_b_mqtt.c` | MQTT session: births, data, deaths, Will, reconnect and rebirth |
| `src/sparkplug_b_espnow_bridge.c` | Translation of the ESP-NOW frames into Sparkplug metrics |
| `src/sparkplug_b_cmd.c` | Interpretation of the NCMD/DCMD commands and translation to ESP-NOW |
| `src/sparkplug_b_device.c` | Registry of the Devices: state, expiry and birth replay |
| `src/sparkplug_b_state.c` | Sequence number and birth-death sequence counters |
| `src/sparkplug_b_topics.c` | Builders of the Sparkplug topic tree |

The component never touches the radio: it translates and hands the frame to
the sink registered by the application with `sparkplug_b_set_espnow_sink()`.

## Alias map

Every alias must be unique across the whole set of metrics of an Edge Node
(`tck-id-payloads-alias-uniqueness`), so the metrics of the node and the ones
of its Devices use disjoint ranges:

| Alias | Metric | Type | Where |
|---:|---|---|---|
| 0 | reserved by the specification | | |
| 1..6 | `Inputs/{Temperature,Ph,Do,Orp,Level,Turbidity}` | Double | DBIRTH/DDATA |
| 7..10 | `Inputs/Custom/0..3` | Double | DBIRTH/DDATA |
| 11 | `Inputs/UptimeMs` | Int64 | NBIRTH (uptime of the EoN-Node) |
| 12 | `Diagnostics/BatteryPercent` | Float | DBIRTH/DDATA |
| 13 | `Diagnostics/RssiLast` | Int8 | DBIRTH/DDATA |
| 14 | `Diagnostics/UptimeMs` | Int64 | DBIRTH/DDATA (uptime of the Device) |
| 15 | `Properties/SleepIntervalSec` | Int64 | DBIRTH (property of the Device) |

A DATA message (NDATA/DDATA) carries the **alias only**: the friendly name was
already bound by the BIRTH message
(`tck-id-payloads-alias-data-cmd-requirement`).

## Verification

* `tools/host-tests/` compiles the Unity test files of `test/` natively on the
  host: **44 tests, 0 failures**.
* `tools/sparkplug-conformance/` asserts the message flow against a real
  Mosquitto broker with the official Tahu schema: **82 checks, 82 passed**.
* `docs/sparkplug_3.0.0-measurements.md` has the flash and DRAM figures, and
  `docs/hardware/esp32-s3-wroom-1-n16r8.md` the hardware blocker that keeps
  the runtime heap unmeasured.

See `odd/tasks/issue-11-sparkplug-b/design.md` for the full design.
