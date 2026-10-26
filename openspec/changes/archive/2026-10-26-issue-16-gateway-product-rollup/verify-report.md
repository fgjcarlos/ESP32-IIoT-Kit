# verify-report — issue #16 gateway product roll-up

**Date**: 2026-10-26
**Tree**: `main` at `8a30f04` plus the changes of this change.
**Environment**: ESP-IDF v5.4 (xtensa-esp32s3), one ESP32-S3-WROOM-1-N16R8 on
`/dev/ttyACM0`, Docker for the conformance harness, QEMU for the logic
profile of the device suite.

## The ledger

Every acceptance criterion of issue #16, once, with the evidence that proves
it or the reason it is not met. Nothing here is asserted beyond what the
commands produced.

| # | Acceptance criterion of issue #16 | Verdict | Evidence |
|---|---|---|---|
| AC-01 | The eight dependent issues are closed with verifiable evidence (build, tests, execution) | **Met** | #8, #9, #10, #11, #12, #13, #14 and #15 are all closed on GitHub. Their evidence was re-produced here: build (§1), host tests 78/78 (§2), device suite 114/114 on the board and 111/111 under QEMU (§3), Sparkplug B conformance 82/82 against a real broker (§4), OTA rollback reproduced on the board (§5), two brand profiles built from the same sources (§6), strict docs build and green CI (§7). |
| AC-02 | The firmware compiles and boots on the ESP32-S3-WROOM-1-N16R8 board with the verified physical carrier | **Partially met** | *Met:* the firmware compiles (`gateway.bin` 799,504 B, 25.42 % of the 3 MiB `ota_0` partition) and boots on the real board: the full initialisation chain runs, the module reports 16 MB of flash and 8 MB of Octal PSRAM (`Found 8MB PSRAM device`, `SPI SRAM memory test OK`), and the main task leaves 5648 bytes of stack headroom. *Not met:* **the carrier is not physically verified.** The pinout comes from a user-supplied image, not from manufacturer documentation. What the board proves is the module, not the carrier's GPIO assignment. See §8. |
| AC-03 | The 1.9" ST7789V2, ESP-NOW, Wi-Fi AP+STA and the optional Sparkplug B work within the memory budget (16 MB flash, 8 MB PSRAM) | **Partially met** | *Met:* Wi-Fi AP comes up (`AP='IIoT-Kit-GW' channel=1`, DHCP server on 192.168.4.1); Sparkplug B is implemented, disabled by default, and passes 82/82 conformance checks against a real Mosquitto broker; the image fits with 2,346,224 bytes free and the 8 MB of PSRAM is detected and tested. *Not met:* **the ST7789V2 and the ESP-NOW do not run.** `lcd_driver`, `lvgl_port`, `ui_screens` and `espnow_manager` are contract-only by design: the display hold forbids a GPIO assignment until the carrier and the panel are physically verified, and ESP-NOW belongs to Fase 2. STA association is also unproven here: there are no site credentials in the local header, so the manager reports `STA disabled (no SSID in credentials)`. See §8. |
| AC-04 | The firmware can be built with brand profiles without duplicating the source | **Met** | `tools/brand/build-profile.sh iiot-kit` and `... acme-industrial` build from the same sources with no source change. The artefacts differ in identity and in digest: `iiot-kit-gw-s3-1-v1.0.0-stable-*.bin` (sha256 `3780f18d…`, AP SSID `IIoT-Kit-GW`, channel `stable`) against `acme-industrial-ax-200-v1.0.0-beta-*.bin` (sha256 `2bebc833…`, AP SSID `Acme-AX200`, channel `beta`). `git diff` over `brand_profile/` and `assets_brand/` between the two builds is empty. |
| AC-05 | OTA rejects incompatible images and demonstrates the rollback | **Met** | *Rejection:* the portable rules are covered by `test_ota_manifest.c` — "A foreign image is refused before it is downloaded", "Release channels never promote a unit by accident", "A downgrade is refused", "Manifest parsing refuses a plain HTTP image", "Manifest parsing refuses an unsupported schema", "The digest of the image is compared with the declared one" — all of them in the 78 host tests. *Rollback:* reproduced on the board (§5): the defective image was armed in `ota_1`, the unit started it, the image never confirmed itself, and the bootloader went back to `ota_0`. |
| AC-06 | The CI and the documentation describe the delivered product on the S3 faithfully | **Met** | `Firmware CI`, `Docs CI` and `Deploy Tutorial to GitHub Pages` are green on `main` (runs 40219836073, 40219835968, 40219836025). `mkdocs build --strict` exits 0 with no warnings; `tools/ci/check-roamlinks.py` reports "every roam link resolves". The docs name the S3-N16R8 with 16 MB of flash and 8 MB of PSRAM, and the display and the carrier are marked as pending physical verification everywhere they appear. |

**Verdict**: four criteria met, two partially met. The two partial ones
depend on a physical verification that cannot be done from a terminal, and
both were the agreed scope of the issues that closed before this roll-up.

## §1 — Build

| Command | Result |
|---|---|
| `idf.py -C firmware/gateway build` | `gateway.bin` 799,504 bytes (0xc3310). Smallest app partition 0x300000; 2,346,224 bytes free; 25.42 % used. |
| `idf.py -C firmware/gateway/test build` | success. |
| `tools/ci/report-size.py` | `fits: true`, `failures: []`. Partition table: nvs / otadata / phy_init / ota_0 / ota_1 / spiffs, 6 partitions, unchanged. |
| `tools/ci/check-no-secrets.sh firmware/gateway/build/gateway.bin` | "no secret found". |
| `tools/ci/check-reproducible.sh` | present as the `reproducible` job of the CI; green on `main`. |

The SPIFFS budget is **not** checked: the dashboard is not built yet, and the
report says so explicitly ("The dashboard has not been built yet, so the
SPIFFS budget is not checked. The budget is 200.00 KiB gzipped").

## §2 — Host tests and node load

```
$ ./build/host-tests/iot_host_tests
78 Tests 0 Failures 0 Ignored
OK
```

```
$ ./build/host-tests/iot_stress --nodes 10 --rounds 50
{ "nodes": 10, "rounds": 50, "frames_total": 500, "frames_accepted": 500,
  "frames_rejected": 0, "loss_percent": 0.000, "identity_errors": 0,
  "sequence_errors": 0, "registry_devices": 10, "registry_errors": 0,
  "rss_before_kib": 1616, "rss_after_kib": 1812, "rss_growth_kib": 196 }
iot_stress: 0 failure(s)
```

The host runner compiles the **same** test files as the on-device project
(`tools/host-tests/CMakeLists.txt` lists the component `test/test_*.c`
sources directly), so a test added there runs in both places.

## §3 — The device suite, in both profiles

| Profile | Where | Result |
|---|---|---|
| Logic (CI) | QEMU | `111 Tests 0 Failures 0 Ignored`, headroom `7504 bytes of 16384`, no crash. |
| Board | `/dev/ttyACM0` | `114 Tests 0 Failures 0 Ignored`, headroom `7504 bytes of 16384`, no crash. |

The three extra tests of the board profile are the `[hw]` ones: the two of
`test_board_profile.c` (the chip model and the 16 MB / 8 MB sizes read from
the hardware, and the allocation of a 108,800-byte framebuffer-sized block
out of the real PSRAM) and the one of `test_board_rgb.c` (the GRB order of
the WS2812, driven through the RMT of GPIO48).

### The defect this roll-up fixed

Before the fix, both profiles ended like this:

```
114 Tests 0 Failures 0 Ignored
OK
I (8943) gateway_test: Main task stack headroom after the run: 0 bytes of 8192
I (8943) main_task: Returned from app_main()
***ERROR*** A stack overflow in task main has been detected.
```

The whole suite runs inside `app_main()`, in the main task, so the stack has
to hold the deepest single test. It did not any more, and the gate in
`tools/ci/run-device-tests.sh` only read the Unity summary, so it reported
success over an application that was crashing.

The fix and its verification:

| Check | Result |
|---|---|
| Stack of 8192 | headroom 0, overflow right after the summary. |
| Stack of 16384 | headroom 7504, no overflow. |
| Gate with a 4096-byte stack (negative test) | `run-device-tests: the main task stack headroom is 0 bytes, below the floor of 1024` → exit 1. |
| Gate with a 2048-byte stack (negative test) | the suite crashes before the summary → `run-device-tests: the Unity summary is missing; the application did not finish` → exit 1. |
| The crash pattern against the real 8192 log text | `grep -E '\*\*\*ERROR\*\*\*\|stack overflow\|…'` matches line 5. |
| Gate with the fixed stack | exit 0, and the summary publishes the headroom line. |

A second, subtler interaction was found while fixing the first: the CI
profile ended the QEMU run because the overflow *crashed* the chip, and the
`-no-reboot` flag stopped the emulation. Once the stack was fixed, the
application returned to the idle task and QEMU ran until the timeout — a
passing run reported as a timeout. The test application now ends the run with
`esp_restart()`, which is how ESP-IDF ends a Unity run. A restart is not a
panic: the panic handler prints `***ERROR***` and `Rebooting...`, and
`esp_restart()` prints neither, so the gate still tells the two apart.

## §4 — Sparkplug B conformance

```
$ bash tools/sparkplug-conformance/scripts/run-conformance.sh
Conformance summary: 82 checks, 82 passed, 0 failed -> PASS
RESULT: conformance harness PASSED (exit 0).
```

Against a real Mosquitto broker in Docker: NBIRTH/NDEATH and DBIRTH/DDEATH
certificates, the sequence space, the `bdSeq` metric, the Tahu payload
decoding, the retain and QoS flags, and the NCMD/DCMD path including the
Node Control/Rebirth metric.

## §5 — The OTA rollback, on the board

```
tools/ota/make-defective-image.sh
  → build/ota-defective/gateway.bin (217,600 bytes), an image whose app_main aborts

esptool.py --chip esp32s3 --port /dev/ttyACM0 --after no_reset write_flash 0x320000 build/ota-defective/gateway.bin
tools/ota/make-otadata.py --slot ota_1 --out /tmp/otadata.bin
  → otadata for ota_1: sequence=2 state=new
esptool.py --chip esp32s3 --port /dev/ttyACM0 --after no_reset write_flash 0xf000 /tmp/otadata.bin
```

The boot after the arming:

```
I (210) boot: Loaded app from partition at offset 0x320000      ← the defective image
… it aborts …
I (155) boot: No factory image, trying OTA 0
I (264) boot: Loaded app from partition at offset 0x20000      ← back to the good image
I (1445) ota_manager: ota: running ota_0, boot ota_0, version 1.0.0, channel stable
```

And the `otadata` partition read back from the board:

```
record 0: seq=2 label='ota_1' state=4 (aborted) crc=0x55f63774 calc=0x55f63774 crc_ok=True
```

`state=4` is `ESP_OTA_IMG_ABORTED`: the bootloader started the defective
image, the image never confirmed itself, and the bootloader discarded it and
went back to `ota_0`.

## §6 — The brand profiles

| Profile | Artefact | Brand | Model | Channel | AP SSID | sha256 |
|---|---|---|---|---|---|---|
| `iiot-kit` | `iiot-kit-gw-s3-1-v1.0.0-stable-8a30f045-dirty.bin` | IIoT-Kit | GW-S3-1 | stable | IIoT-Kit-GW | `3780f18d…` |
| `acme-industrial` | `acme-industrial-ax-200-v1.0.0-beta-8a30f045-dirty.bin` | Acme Industrial | AX-200 | beta | Acme-AX200 | `2bebc833…` |

Both come from the same tree. `git diff` over `components/brand_profile/` and
`components/assets_brand/` between the two builds is empty: the identity is
a build-time choice, not a fork.

## §7 — Documentation and CI

| Check | Result |
|---|---|
| `mkdocs build --strict` | exit 0, no warnings. |
| `python3 tools/ci/check-roamlinks.py` | "OK: every roam link resolves", exit 0. |
| `Firmware CI` on `main` | success (40219836073). |
| `Docs CI` on `main` | success (40219835968). |
| `Deploy Tutorial to GitHub Pages` | success (40219836025). |

## §8 — What is deferred, and why

Two items are not met, and both are the same kind of item: **they need the
physical unit in the developer's hands.**

### The physical carrier

The carrier pinout is a user-supplied image, not manufacturer documentation.
`docs/hardware/esp32-s3-wroom-1-n16r8.md` is the source of truth and marks
it as unverified, and `openspec/config.yaml` → `open_constraints` forbids
committing a GPIO assignment until the carrier board and the module are
verified. The connected board proves the module: 16 MB of flash, 8 MB of
Octal PSRAM, the WS2812 on GPIO48, the AP. It does not prove which carrier
pin goes where.

**To close it**: compare the delivered carrier against the pinout table of
`docs/hardware/esp32-s3-wroom-1-n16r8.md`, with a meter and a jumper, and
record the result there. Until then, AC-02 stays partial.

### The display and the radio

`lcd_driver`, `lvgl_port`, `ui_screens` and `display_manager` are
contract-only. No `esp_lcd_*` call, no `lv_init`, no draw buffer, no tick
timer, no GPIO, no ST7789 register. The header of `lcd_driver` states the
hold verbatim: the 8-pin contact map is display-side and not a GPIO
assignment, GPIO22 is a candidate for the backlight pending verification,
and the withdrawn `0.1155 mm` pitch is not used.

`espnow_manager` is a contract too: the interface exists, the runtime does
not. ESP-NOW is scheduled in Fase 2, and the phases are sequential.

**To close it**: verify the carrier and the panel, then implement the
display bring-up in the Fase that owns it, and implement ESP-NOW in Fase 2.

### What is *not* deferred

Everything else in the issue is met and demonstrated above, including the
parts that are usually the excuse to leave something open: the build fits,
the suite runs on the hardware, the conformance harness passes against a
real broker, the rollback happens on the board, and the two brand profiles
come out of one source tree.

## Residual risks

| Risk | Note |
|---|---|
| The main task stack of the test application is sized by a measurement | The gate fails the run below a floor of 1024 bytes, so the next growth of the suite is caught as a failure and not as a crash. |
| A stale generated `sdkconfig` keeps the old stack | The headroom floor catches it: with the old 8192 the headroom is 0 and the gate fails. The documented board command removes the generated file. |
| The board profile restarts the unit after the suite | The unit boots the test application again and runs the suite again. That is the behaviour the overflow produced before, and it is what the CI expects from the `-no-reboot` flag. |
| The conformance harness needs Docker | It runs in the CI as its own job; locally it needs a Docker daemon. |
