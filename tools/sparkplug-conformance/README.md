# Sparkplug B conformance harness

A self-contained conformance harness for the gateway's Sparkplug B v3.0.0 mode.
It runs a throwaway Mosquitto broker plus a Python client that plays the two
MQTT roles of a Sparkplug deployment:

* a **Host Application**, which subscribes to the Sparkplug namespace, receives
  the messages and asserts them;
* a simulated **Edge Node** (`simulate` mode only), which publishes the birth,
  data and death messages and registers the MQTT **Will** that carries the
  NDEATH.

This closes **CA-S6** of [issue #11](../../issues/11):

> Harness de conformidad con Mosquitto + cliente Python que aserta los 8 mensajes.

The harness is deliberately **not** the commercial Ignition conformance suite.
It asserts the normative rules of the Eclipse Sparkplug B v3.0.0 specification
against the official Tahu schema (see
[Known limitations](#known-limitations)).

---

## Layout

```
tools/sparkplug-conformance/
├── README.md
├── docker-compose.yml          # broker + client stack
├── mosquitto/
│   └── mosquitto.conf          # test-only broker configuration
├── client/
│   ├── Dockerfile              # builds the image, compiles the Tahu schema
│   ├── requirements.txt
│   ├── conformance.py          # Host Application + simulated Edge Node
│   └── tahu/
│       ├── __init__.py
│       ├── .gitignore          # ignores the generated bindings
│       └── sparkplug_b.proto   # vendored upstream Eclipse Tahu schema
└── scripts/
    └── run-conformance.sh      # entry point
```

---

## Prerequisites

* **Docker** with **Compose** (`docker compose`, or the legacy `docker-compose`).
  The script detects which one is available.
* Nothing else for `simulate` mode — no ESP32, no broker outside the stack.

For `observe` mode (asserting a **real** ESP32-S3 gateway) you additionally need:

* the gateway flashed with `CONFIG_SPARKPLUG_B_ENABLED=y` and running, either
  attached over USB (`/dev/ttyACM0`) or reachable over the network by IP;
* the gateway configured with a broker URI it can actually reach.

### Container networking caveat (read this before using `observe`)

The `client` container lives on a Docker bridge network. The **broker** in this
stack is published on the host, so the container reaches it as `mqtt://broker:1883`
over the compose network — that always works.

The **physical gateway**, however, is not on the compose network. It can only
reach a broker that is published on the host's reachable addresses. Two
supported layouts:

1. **Broker published on the host** (default). The gateway connects to the
   host's LAN address, e.g. `mqtt://192.168.1.50:1883`. The container client
   keeps using `mqtt://broker:1883` — no change needed.
2. **Broker running elsewhere on the host or LAN.** Point the container at it
   with `SPARKPLUG_BROKER_URI`. To reach a broker published on the host from
   inside the container, use the `host.docker.internal` alias, which is already
   wired up in `docker-compose.yml` via:

   ```yaml
   extra_hosts:
     - "host.docker.internal:host-gateway"
   ```

   ```bash
   SPARKPLUG_BROKER_URI=mqtt://host.docker.internal:1883 MODE=observe \
       bash tools/sparkplug-conformance/scripts/run-conformance.sh
   ```

   If the container network cannot route to the broker at all, run the client
   with `--network host` instead of the compose network.

---

## How to run

### Simulate mode (default)

Self-contained: the simulated Edge Node publishes a spec-valid flow and the Host
Application asserts what the broker delivered. This is the mode that can run in
CI.

```bash
bash tools/sparkplug-conformance/scripts/run-conformance.sh
```

If port 1883 of the host is already taken by another broker, publish it on
another port:

```bash
SPARKPLUG_BROKER_HOST_PORT=18830 \
    bash tools/sparkplug-conformance/scripts/run-conformance.sh
```

The run ends by dropping the TCP connection of the Edge Node **without** an
MQTT DISCONNECT, so the broker publishes the Will on its behalf. That is how a
Host Application receives the NDEATH of an Edge Node that died unexpectedly, and
it is the only way to exercise the Will path for real.

### Observe mode (real gateway)

Asserts what the physical ESP32-S3 gateway publishes. The harness publishes
nothing except an optional rebirth probe; it waits for `NBIRTH`, `DBIRTH` and
`DDATA` (and optionally `NDATA` and `DDEATH`) and fails if they do not arrive.

```bash
# 1. start the broker only, so the gateway can connect to it
docker compose up -d broker

# 2. point the gateway at the broker (e.g. mqtt://<host-lan-ip>:1883),
#    flash and reset the ESP32-S3, then run the assertion
MODE=observe SPARKPLUG_TIMEOUT=60 \
    bash tools/sparkplug-conformance/scripts/run-conformance.sh
```

`observe` mode is tolerant of message **ordering** — a `DDATA` may arrive
before or after the `DBIRTH` — but strict about the correctness of every
individual message.

#### A gateway without sensor nodes

`DBIRTH` and `DDATA` describe a **Device**, and a Device only exists once a
sensor node has been discovered through ESP-NOW. A gateway that has not
discovered a node yet therefore cannot publish them, and the observe mode
would fail on messages that are correct to be absent.

For that case the Device messages are not required:

```bash
MODE=observe SPARKPLUG_EXPECT_DEVICE=0 SPARKPLUG_EXPECT_NDATA=1 \
    SPARKPLUG_TIMEOUT=60 \
    bash tools/sparkplug-conformance/scripts/run-conformance.sh
```

The client also accepts `--no-expect-device` when it runs outside Compose.
Everything else stays asserted: the payload of every received message, the
sequence space, the alias uniqueness and the QoS and retain flags.

### Running the client alone

```bash
cd tools/sparkplug-conformance/client
python -m grpc_tools.protoc --proto_path=./tahu --python_out=./tahu ./tahu/sparkplug_b.proto
python conformance.py --help
python conformance.py --mode simulate --broker mqtt://localhost:1883
```

---

## Configuration

All settings are environment variables; the CLI flags override them.

| Variable | CLI flag | Default | Meaning |
|---|---|---|---|
| `MODE` | `--mode` | `simulate` | `simulate` or `observe` |
| `SPARKPLUG_BROKER_URI` | `--broker` | `mqtt://broker:1883` | Broker URI |
| `SPARKPLUG_GROUP_ID` | `--group-id` | `iiot-kit` | Sparkplug group id |
| `SPARKPLUG_EDGE_NODE_ID` | `--edge-node-id` | `GW-TEST01` | Edge node id |
| `SPARKPLUG_DEVICE_ID` | `--device-id` | `DEV-TEST01` | Device id (simulate mode) |
| `SPARKPLUG_TIMEOUT` | `--timeout` | `30` | Seconds to wait for broker and messages |
| `SPARKPLUG_BROKER_HOST_PORT` | — | `1883` | Host side of the published broker port |
| `SPARKPLUG_EXPECT_NDATA` | `--expect-ndata` | `0` | Observe mode also requires an `NDATA` |
| `SPARKPLUG_EXPECT_DDEATH` | `--expect-ddeath` | `0` | Observe mode also requires a `DDEATH` |
| `SPARKPLUG_EXPECT_REBIRTH` | `--expect-rebirth` | `0` | Observe mode probes a rebirth |

`SPARKPLUG_BROKER_HOST_PORT` only matters when port 1883 is already taken on
the machine by another broker; the container-internal port never changes.

### Exit codes

| Code | Meaning |
|---|---|
| `0` | Every assertion passed |
| `1` | Harness error — broker unreachable, malformed configuration, ... |
| `2` | At least one assertion failed |

---

## What the harness asserts

The requirement ids below are the ones of the Eclipse Sparkplug B v3.0.0
specification.

### Transport and topics

* The broker accepts every publish and acknowledges it.
* Every received topic parses as a Sparkplug topic: the `spBv1.0` namespace
  prefix, a group id, a known message type, and the correct number of levels
  (4 for node topics, 5 for device topics).
* The expected topic is used exactly — e.g. `spBv1.0/{group}/NBIRTH/{edge}`
  and `spBv1.0/{group}/DBIRTH/{edge}/{device}`.

### Payload shape

* The payload decodes cleanly with the official Tahu schema.
* `BIRTH` messages carry at least one metric, and every metric defines a
  **name**, an **alias** and a **datatype**
  (`tck-id-payloads-alias-birth-requirement`).
* The `bdSeq` metric has the INT64 datatype, carries a value and **no alias**,
  matching the reference example of the specification.
* The `Node Control/Rebirth` metric has the Boolean datatype, the value
  `false` and **no alias**
  (`tck-id-operational-behavior-data-commands-rebirth-datatype`, `-value`,
  `-name-aliases`).
* `DATA` messages carry at least one metric, every metric references an
  **alias** and **no metric carries a name**
  (`tck-id-payloads-alias-data-cmd-requirement`).
* Every alias supplied by a BIRTH message is **unique across the whole metric
  set of the Edge Node**
  (`tck-id-payloads-alias-uniqueness`): the metrics of the node and the ones of
  its devices live in disjoint alias ranges.
* `NDEATH` carries **only** the `bdSeq` metric and **no** sequence number
  (`tck-id-topics-ndeath-payload`, `tck-id-topics-ndeath-seq`).
* `DDEATH` carries a sequence number and the `bdSeq` metric
  (`tck-id-payloads-ddeath-seq-number`).

### QoS and retain flags

The Host Application subscribes at QoS 1, and MQTT delivers at
`min(publish QoS, subscription QoS)`, so the delivered QoS reveals the QoS the
publisher used:

* `NBIRTH`, `DBIRTH`, `NDATA`, `DDATA` and `DDEATH` are published at **QoS 0**
  with the **retain flag set to false**.
* The `NDEATH` is the MQTT **Will** of the Edge Node: **QoS 1** with the
  **retain flag set to false**
  (`tck-id-payloads-ndeath-will-message-qos`, `-will-message-retain`).
* The `NCMD` and `DCMD` subscriptions of the Edge Node are made at **QoS 1**
  (`tck-id-message-flow-edge-node-birth-publish-ncmd-subscribe`).

### Sequence semantics

* There is **one sequence space per Edge Node**: the node and all of its
  devices share it. Every message published after an `NBIRTH` carries the
  previous sequence number plus one, wrapping from 255 to 0
  (`tck-id-payloads-sequence-num-incrementing`).
* An `NBIRTH` opens a new sequence space and carries a sequence number between
  0 and 255 (`tck-id-payloads-sequence-num-zero-nbirth`).
* The `NDEATH` is the only Sparkplug message **without** a sequence number
  (`tck-id-payloads-sequence-num-always-included`).
* A duplicate delivery (QoS 1 redelivery) is recognised and ignored rather
  than reported as a sequence violation.

### Birth / death sequence

* The `bdSeq` of the `NDEATH` **matches** the `bdSeq` of the `NBIRTH` of the
  same session (`tck-id-payloads-ndeath-bdseq`).

### Command round trip

* An `NCMD` is published to `spBv1.0/{group}/NCMD/{edge}` and reaches the Edge
  Node at QoS 1 with its command metric intact.
* A `DCMD` is published to `spBv1.0/{group}/DCMD/{edge}/{device}` and reaches
  the Edge Node at QoS 1 with its command metric intact.
* A Rebirth Request carries a metric **named** `Node Control/Rebirth` with the
  value `true`
  (`tck-id-operational-behavior-data-commands-ncmd-rebirth-verb`, `-name`,
  `-value`).

### Optional rebirth probe (`observe` mode only)

`--expect-rebirth` publishes a Rebirth Request and requires a **second**
`NBIRTH` whose `bdSeq` is **unchanged**
(`tck-id-operational-behavior-data-commands-rebirth-action-3`).

---

## Vendored schema: license and provenance

`client/tahu/sparkplug_b.proto` is the **official Eclipse Tahu Sparkplug B
schema**, vendored verbatim.

| | |
|---|---|
| Upstream | <https://github.com/eclipse/tahu/blob/3.x/sparkplug_b/sparkplug_b.proto> |
| Obtained from | `https://raw.githubusercontent.com/eclipse/tahu/3.x/sparkplug_b/sparkplug_b.proto` on 2026-10-24 |
| SHA-256 of the upstream file | `4432c5c483b7fb9732d0594c98a2e97dca5e517e39c5374a8b918d837f0b4a19` |
| License | EPL-2.0 (as stated in the file header); Apache-2.0 applies to the non-EPL portions of the upstream Tahu distribution |
| Proto package | `org.eclipse.tahu.protobuf` (proto2) |

The file was fetched from upstream and is byte-for-byte unmodified below the
provenance header that was added on top of the original copyright block. It is
**not** a hand-written approximation.

Python bindings are generated at **image build time** with `grpc_tools.protoc`
(see `client/Dockerfile`); the generated `sparkplug_b_pb2.py` is therefore not
committed. The Tahu schema is proto2, so the generated bindings expose
`HasField()` and `WhichOneof("value")` — the harness relies on both.

---

## Known limitations

* **This is not the commercial Ignition conformance suite.** It checks the
  message shape, the topic namespace, the QoS and retain flags, the alias and
  sequence semantics and the Will behaviour. It does not implement the full
  Sparkplug B state machine, the `STATE` topic, metric templates, or the
  complete datatype matrix.
* **Data types are only partially checked.** The harness asserts that a
  datatype is declared and that the value is present; it does not verify that
  the declared datatype matches the value field of the `oneof`.
* **The device metric set is declared up front.** The ESP-NOW discovery frame
  does not carry the number of values of the sensor, so the gateway cannot know
  in advance which `Inputs/Custom/*` slots a device will publish. It therefore
  declares the whole Custom range in the `DBIRTH`. The harness reports any
  alias published in a `DATA` message that no `BIRTH` declared as an
  informational line, so the gap stays visible if the protocol ever changes.
* **Command metrics are identified by name, not by alias.** The specification
  requires command metrics to carry an alias only, but the Rebirth Request is
  the documented exception: it is identified by the metric name
  `Node Control/Rebirth`, because that metric is not aliased in the `NBIRTH`.
  The harness follows the specific rule for the control metrics.
* **`DDEATH` is not required by default in `observe` mode**, because a device
  only emits it on expiry or on an unclean disconnect. Use `--expect-ddeath`
  to require it, and `--expect-ndata` for the `NDATA`.
* **The broker is insecure by design** — no authentication, no TLS, plaintext
  listener, no persistence. It is disposable and must never be exposed to an
  untrusted network.
* **MQTT 3.1.1 only.** No v5 listener is provided, matching the gateway's
  pinned MQTT version.
