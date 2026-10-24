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

Measured with the build of 2026-10-24: ESP-IDF v5.4, target `esp32s3`,
`CONFIG_ESP_MAIN_TASK_STACK_SIZE=8192` and the secondary USB-Serial/JTAG
console that ESP-IDF enables by default on this target.

| Configuration | `gateway.bin` | Free in the 3 MiB OTA slot |
|---|---:|---:|
| Sparkplug B off (default) | 779,168 B | 75 % |
| Sparkplug B on | 1,001,040 B | 68 % |
| **Delta** | **+221,872 B** | |

The delta comes mostly from the TLS stack that the MQTT transport pulls in
(`libmbedtls.a` 101,259 B, `libmqtt.a` 16,030 B), not
by the Sparkplug code itself.

The delta is dominated by the TLS stack that the MQTT client needs
(`libmbedtls.a` 101,259 B, `libmqtt.a` 16,030 B), not
by the Sparkplug code itself.

## Memory summary, Sparkplug B on

ESP-IDF v5.4 reports the internal RAM of this target as `DIRAM` (data, bss and
the IRAM text) plus `IRAM` (the 16 KB instruction RAM) separately:

| Memory type | Used | Used % | Remain | Total |
|---|---:|---:|---:|---:|
| Flash Code | 665,462 | | | |
| Flash Data | 222,024 | | | |
| DIRAM | 123,651 | 36.18 % | 218,109 | 341,760 |
| IRAM | 16,383 | 99.99 % | 1 | 16,384 |
| RTC FAST | 52 | 0.63 % | 8,140 | 8,192 |

With the feature off, DIRAM drops to 112,183 B (32.83 %), so the Sparkplug
integration costs **11,468 B of static DIRAM**.

The IRAM is at 99.99 % in **both** configurations, so it is not caused by the
Sparkplug integration: the Wi-Fi driver and the secondary USB-Serial/JTAG
console fill it. It leaves one byte of headroom, which is worth keeping in
mind before adding more code to the gateway.

## The Sparkplug component itself

```
libsparkplug_b.a   Total 19,025 B   DRAM 9,544 B (.bss)   Flash code 8,293 B   Flash data 1,188 B
```

| Archive | Total | DRAM | Flash code | Flash data |
|---|---:|---:|---:|---:|
| `libsparkplug_b.a` | 19,025 | 9,544 | 8,293 | 1,188 |
| `libmqtt.a` | 16,030 | 0 | 15,696 | 334 |
| `libmbedtls.a` | 101,259 | 270 | 28,787 | 72,202 |
| `libprotocol.a` | 247 | 0 | 247 | 0 |

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
component to 36,988 B of DRAM.

The buffers are now allocated on demand, only when the broker connection
actually uses TLS, and released on stop. The measured effect on the component:

| | Before | After | Saved |
|---|---:|---:|---:|
| `libsparkplug_b.a` DRAM | 36,988 B | 9,544 B | 27,444 B |
| Static TLS buffers in DRAM | 24,576 B | 0 B | 24,576 B |

The rows of the table above that used to quote the total DRAM of the gateway
were measured on an earlier baseline (a build without the secondary
USB-Serial/JTAG console and with the 3584-byte main task), so they are not
comparable with the current figures. The current baseline is the DIRAM row of
the memory summary, and the current image size is the one of the image size
table.

## Test image

| Configuration | `gateway_test.bin` | Free in the 1.5 MiB factory partition |
|---|---:|---:|
| Unity test project | 823,488 B | 48 % |

## Runtime heap

**Measured on the connected unit on 2026-10-24.** The on-device Unity project
reports the free heap before it allocates the RGB565 framebuffer-sized block:

| Measurement | Result |
|---|---:|
| Free internal heap | 311,967 bytes |
| Free PSRAM | 8,386,148 bytes |
| Largest PSRAM block | 8,257,536 bytes |
| Main task stack headroom after the run | 656 bytes of 8192 |

These values come from the Unity test application, not from the full gateway
firmware: the gateway adds the MQTT session, the TLS material and the display
chain on top of them. The heap **after an established MQTT session** is still
open, because the session needs a broker that the board can reach.

## On-device evidence

The connected ESP32-S3 now boots the firmware and runs the tests. What the
board reported:

| Observation | Result |
|---|---|
| Boot reason after `idf.py flash` | `rst:0x15 (USB_UART_CHIP_RESET),boot:0x8 (SPI_FAST_FLASH_BOOT)` |
| Unity run on the board | `81 Tests 0 Failures 0 Ignored`, `OK` |
| Sparkplug session task | `Sparkplug B session task started (group=iiot-kit edge=GW-TEST01)` |
| Broker reachable from the board | Not yet: the board has no STA credentials, so it reaches the broker through its own AP |

The last row is the open item: the gateway has no Wi-Fi credentials on this
unit, so the MQTT session stays in its reconnect backoff until the host joins
the access point of the board (`IIoT-Gateway`) and the broker is reachable at
`192.168.4.2:18830`.

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
  Current result: **46 tests, 0 failures**.
- `tools/sparkplug-conformance/` — Sparkplug B conformance harness against a
  real Mosquitto broker.
- `tools/board/boot_from_flash.py` — restarts the board with the reset of the
  ESP-IDF monitor and captures its console over the native USB port.
