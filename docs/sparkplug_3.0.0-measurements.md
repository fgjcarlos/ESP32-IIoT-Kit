# Sparkplug B v3.0.0 — measurements

Measurements for the Sparkplug B work of issue #11, taken with ESP-IDF v5.4
on the `esp32s3` target (`firmware/gateway`).

Everything here is a **build-time** measurement of the gateway image. The
connected development unit could not execute the firmware — its GPIO46 strap
is held low, so the ROM only reaches the downloader (see
`docs/hardware/esp32-s3-wroom-1-n16r8.md`). The runtime heap figures that
the issue asks for are therefore **not** measured, and this document says so
explicitly instead of estimating them.

## Build under test

| Setting | Value |
|---|---|
| Toolchain | ESP-IDF v5.4, xtensa-esp32s3-elf (GCC 14.2.0) |
| Target | `esp32s3` |
| Flash | 16 MB Quad-SPI, custom partition table (`partitions.csv`) |
| PSRAM | 8 MB Octal-SPI, enabled (`CONFIG_SPIRAM_MODE_OCT=y`) |
| Feature flag | `CONFIG_SPARKPLUG_B_ENABLED=y` |
| Command | `idf.py build && idf.py size && idf.py size-components` |

The flag defaults to **off** in `sdkconfig.defaults`, so the Sparkplug code is
not linked into the image unless it is explicitly enabled. Both numbers
matter: the off state is what ships by default, the on state is what the
feature costs.

## Image size

| Configuration | `gateway.bin` | Total image | Free in the 3 MiB OTA slot |
|---|---:|---:|---:|
| Sparkplug B off (default) | 830,000 B | 830,256 B | 74 % |
| Sparkplug B on | 1,050,128 B | 1,050,384 B | 67 % |
| **Delta** | **+220,128 B** | **+220,128 B** | |

The delta is dominated by the TLS stack that the MQTT client needs
(`libmbedtls.a` 101,938 B, `libmqtt.a` 15,038 B, `libesp-tls.a` 6,467 B), not
by the Sparkplug code itself.

## Memory summary, Sparkplug B on

| Memory type | Used | Used % | Remain | Total |
|---|---:|---:|---:|---:|
| Flash Code | 686,933 | | | |
| Flash Data | 233,884 | | | |
| IRAM | 112,670 | 85.96 % | 18,402 | 131,072 |
| DRAM | 44,756 | 24.76 % | 135,980 | 180,736 |
| RTC FAST | 28 | 0.34 % | 8,164 | 8,192 |
| RTC SLOW | 24 | 0.29 % | 8,168 | 8,192 |

With the feature off, DRAM drops to 33,604 B (18.59 %), so the Sparkplug
integration costs **11,152 B of static DRAM**.

## The Sparkplug component itself

```
libsparkplug_b.a   Total 18,956 B   DRAM 9,544 B (.bss)   Flash code 8,224 B   Flash data 1,188 B
```

| Archive | Total | DRAM | Flash code | Flash data |
|---|---:|---:|---:|---:|
| `libsparkplug_b.a` | 18,956 | 9,544 | 8,224 | 1,188 |
| `libmqtt.a` | 15,038 | 0 | 14,704 | 334 |
| `libesp-tls.a` | 6,467 | 4 | 6,371 | 92 |
| `libmbedtls.a` | 101,938 | 270 | 29,466 | 72,202 |
| `libprotocol.a` | 246 | 0 | 246 | 0 |

### What the 9,544 B of DRAM is

| Symbol | Bytes | Note |
|---|---:|---|
| `s_devices` | 6,080 | Device registry: 20 devices x 304 B (id, MAC, state, timestamp and the metric set kept for the birth replay) |
| `s_decoded` | 1,920 | 16 decoded metrics x 120 B |
| `s_group_id`, `s_edge_node_id`, `s_uri`, `s_username`, `s_password` | 1,280 | 5 NVS configuration copies of 256 B |
| `s_ni` and the remaining session state | ~264 | |

The registry is sized by the ESP-NOW peer limit (20 devices). It stores the
*metric set* of each DBIRTH instead of the encoded bytes, because a rebirth
has to publish a DBIRTH with a fresh sequence number: replaying frozen bytes
would repeat the sequence number of the previous session. That change made
the registry 1,760 B smaller than the byte-buffer version it replaced.

### A measurement that changed the design

The first implementation kept the TLS material in three static buffers of
8,192 B each. That alone cost **24,576 B of DRAM in every build**, including
the plaintext deployments that never touch them, and it pushed the Sparkplug
component to 36,988 B of DRAM and the image to 72,196 B of total DRAM
(39.95 %).

The buffers are now allocated on demand, only when the broker connection
actually uses TLS, and released on stop. The measured effect:

| | Before | After | Saved |
|---|---:|---:|---:|
| `libsparkplug_b.a` DRAM | 36,988 B | 9,544 B | 27,444 B |
| Total DRAM | 72,196 B (39.95 %) | 44,756 B (24.76 %) | 27,440 B |
| Flash | 1,049,543 B | 1,050,384 B | +841 B |

## Test image

| Configuration | `gateway_test.bin` | Free in the 1.5 MiB factory partition |
|---|---:|---:|
| Unity test project | 817,536 B | 48 % |

## Runtime heap

**Not measured.** The issue asks for the heap consumption on device, and the
connected unit cannot run the firmware, so there is no measurement to report.
The numbers above are static allocation from the linker map; the runtime heap
after the MQTT session, the TLS handshake and the display chain are still
open.

What is needed to close it, once the GPIO46 strap is fixed:

```bash
idf.py -p /dev/ttyACM0 flash monitor
```

and read the `heap_caps_get_free_size` / `esp_get_free_heap_size` values from
the boot log, with and without an established MQTT session.

## Reproducing

```bash
cd firmware/gateway
idf.py build
idf.py size
idf.py size-components
```

For the feature-off baseline, set `CONFIG_SPARKPLUG_B_ENABLED` to `n` in
`sdkconfig` and rebuild.

## Related

- `tools/host-tests/` — runs the same Unity test files natively on the host.
  Current result: **44 tests, 0 failures**.
- `tools/sparkplug-conformance/` — Sparkplug B conformance harness against a
  real Mosquitto broker.
