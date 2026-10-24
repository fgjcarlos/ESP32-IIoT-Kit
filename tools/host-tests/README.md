# Host test runner for the gateway's portable logic

This folder builds the gateway's Sparkplug B logic **natively on the host**
and runs the same Unity test files the on-device test project runs.

## Why it exists

The gateway firmware targets the ESP32-S3, and the project's test project
(`firmware/gateway/test/`) builds a Unity app that runs on the device. The
connected development unit could not boot from flash — its GPIO46 strap is
held low, so the ROM only reaches the downloader (see
`docs/hardware/esp32-s3-wroom-1-n16r8.md`). The on-device runner was
therefore not executable, and the test files had never been run.

Compiling the same test files against the same component sources on the host
executes them anyway, and it turns the suite into a fast pre-commit gate for
the logic that does not need a radio.

The build is **not** a second copy of the tests: `CMakeLists.txt` compiles
`firmware/gateway/components/sparkplug_b/test/*.c` directly, so a test added
there runs in both places.

## What is covered, and what is not

| Area | Covered on the host | Needs the device |
|---|---|---|
| Protobuf payload encode/decode | yes | — |
| NCMD/DCMD interpretation and ESP-NOW translation | yes | — |
| Device registry, expiry policy, birth replay | yes | — |
| seq and bdSeq state machine | yes | — |
| ESP-NOW → Sparkplug bridge | yes | — |
| MQTT session, Will, birth/death publication | no | yes |
| Radio transmit, display, Wi-Fi | no | yes |

The MQTT transport is not part of the host build: it depends on the ESP-IDF
MQTT client and on FreeRTOS tasks. Its behaviour is covered by the
conformance harness in `tools/sparkplug-conformance/`, which runs against a
real Mosquitto broker.

## Requirements

- A C compiler and CMake
- An ESP-IDF checkout (only for the portable Unity core and its Linux port).
  The default location is `$HOME/esp/esp-idf`; override it with
  `-DIDF_ROOT=/path/to/esp-idf`.
- The nanopb sources that the component vendors as a managed component. They
  are not committed, so run the firmware build once before the host build:

  ```bash
  cd firmware/gateway && idf.py build
  ```

`tools/host-tests/shims/` provides the handful of ESP-IDF symbols the
components use (`esp_err.h`, `esp_log.h`, `esp_timer.h`, `esp_system.h`,
`nvs.h`, `nvs_flash.h` and the FreeRTOS mutex API). The shims are deliberately
thin: the code under test is the real component source, unmodified.

## Running

```bash
cmake -S tools/host-tests -B build/host-tests
cmake --build build/host-tests
./build/host-tests/iot_host_tests
```

The runner exits `0` when every test passes and `1` otherwise, so it can be
used directly in CI.

Output ends with the usual Unity summary:

```
-----------------------
38 Tests 0 Failures 0 Ignored
OK
```
