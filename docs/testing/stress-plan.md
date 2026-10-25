# Stress plan — ten nodes and more

The gateway is the only device that stays awake: it has to hold the nodes
behind it at the same time, keep their identities apart, and publish their
data without losing the sequence. A bug that only shows up with several nodes
is invisible to a unit test with one node, so the stress test is a first-class
part of the test suite and not a manual ritual.

The plan has two halves, and they answer different questions:

| Half | Where it runs | What it proves |
|---|---|---|
| [A. Node load](#a-node-load-the-state-of-the-gateway) | The host, in the CI | The gateway state survives many nodes: registry, identities, sequence space, memory |
| [B. Radio soak](#b-radio-soak-the-real-air) | The board, with real nodes | The radio carries the load: loss, RSSI, latency, the energy of the nodes |

Half A is automated and runs on every change. Half B needs hardware and is
listed in the [hardware test catalogue](hardware-tests.md) as `HW-STRESS`.

## A. Node load: the state of the gateway

This is the executable part. `tools/host-tests/stress_nodes.c` drives the real
bridge, the real protocol codec and the real device registry with N synthetic
nodes, at the same point where the ESP-NOW receive callback injects the
frames.

### How to run it

```bash
source $HOME/esp/esp-idf/export.sh
cmake -S tools/host-tests -B build/host-tests
cmake --build build/host-tests -j

# The acceptance run of the plan: ten nodes.
./build/host-tests/iot_stress --nodes 10 --rounds 50 --json build/stress-10-nodes.json

# The headroom of the registry: twenty nodes, the ESP-NOW peer limit.
./build/host-tests/iot_stress --nodes 20 --rounds 200
```

The harness exits with 1 when an acceptance criterion is not met, so the CI
runs it as a gate.

### What it measures

| Measure | Acceptance criterion | Why |
|---|---|---|
| `frames_rejected` | 0 | A frame the gateway cannot translate is a lost reading |
| `loss_percent` | 0 % | The same, as a ratio |
| `identity_errors` | 0 | Every frame must carry the device id of the node that sent it |
| `registry_errors` | 0 | Every node must be in the registry with its own MAC |
| `registry_devices` | exactly `nodes` | The registry must not collapse entries |
| `sequence_errors` | 0 | The sequence space advances once per published message and wraps at 255 |
| `rss_growth_kib` | ≤ 8192 | Ten nodes must not cost megabytes of heap |

### What it does NOT prove

The harness injects the frames, so it does not cover the radio: the loss of
the air, the collisions, the ESP-NOW peer limit of 20, the channel of the
Wi-Fi and the energy of the nodes. That is half B.

## B. Radio soak: the real air

Ten ESP32-C3 nodes report to one gateway, for 24 hours.

### Setup

| Item | Value |
|---|---|
| Nodes | 10 ESP32-C3, nodes 1 to 10, each with its own MAC recorded |
| Gateway | 1 ESP32-S3-WROOM-1-N16R8, AP mode, fixed channel (the same one ESP-NOW uses) |
| Cadence | One report every 5 minutes, as the battery target of the node |
| Duration | 24 hours of continuous operation (288 reports per node) |
| MQTT | Optional but recommended: the broker records what the Host Application sees |

### Procedure

1. Flash every node with the node firmware and record its MAC in the table of
   the run sheet.
2. Start the gateway, confirm the AP is up and the channel is the fixed one.
3. Start the broker and the bridge, so the whole chain is measured.
4. Leave it running for 24 hours. Do not touch the gateway.
5. Collect the evidence.

### Evidence to collect

| # | Measure | How | Acceptance criterion |
|---|---|---|---|
| 1 | Reports received per node | The counter of the gateway, per node | 288 ± 2 for a 24 h run |
| 2 | Packet loss | Reports sent minus reports received, per node | < 1 % per node |
| 3 | Sequence gaps | The `seq` of the consecutive reports of a node | No gap bigger than 1 |
| 4 | RSSI | The last RSSI of every report | Above the sensitivity of the board (see `docs/hardware/esp32-s3-wroom-1-n16r8.md`) |
| 5 | Heap of the gateway | The free heap logged every hour | No downward trend over the 24 h |
| 6 | Registry | The number of devices the gateway knows | 10, and no duplicate |
| 7 | Stale devices | The devices declared dead | Only the nodes that were really off |
| 8 | Wi-Fi stability | The association of the gateway | No reconnect during the run |

### Pass criteria

The soak passes when every node keeps its identity, the loss stays below 1 %,
no sequence gap is bigger than one, the heap does not trend downwards and the
registry holds exactly ten devices at the end of the run.

### How to scale it

The plan is written for ten nodes because that is the number the issue asks
for, and because it is the number the registry and the ESP-NOW peer limit
(20) can absorb. To go further:

- 20 nodes: the upper bound of the registry (`SPARKPLUG_B_MAX_DEVICES`). The
  host harness covers this case with `--nodes 20`.
- More than 20: the gateway has to page the devices. That is a different
  design and not covered here.

## The CI gate

The CI runs the node load of this plan on every change of the firmware:

```yaml
- name: Node load (10 nodes)
  run: ./build/host-tests/iot_stress --nodes 10 --rounds 50 --json build/stress-10-nodes.json
```

The job fails when the harness reports a failure. The report is published as
an artefact of the run.
