# explore — issue #16 gateway product roll-up

## Objective

Issue #16 is the umbrella proposal of the product: *build an IIoT gateway on
the ESP32-S3-WROOM-1-N16R8 with a local ST7789V2 screen and a
brandable firmware*. It declares eight dependent issues (#8 … #15), an
acceptance criteria list that rolls them up, and a scope that explicitly
excludes migrating the nodes, adding Zigbee/Thread/ML/mesh and forking the
firmware per customer.

This change does **not** implement a subsystem. It closes the roll-up:
it audits every dependent issue against real evidence, fixes the defect the
audit exposed, and records which acceptance criteria are met, which are met
under a hardware hold, and which remain open.

## State of the repository at the start of the change

| Item | State |
|---|---|
| Open issues | #16 only. #8–#15 and #21 are closed. |
| Branch | `main` at `8a30f04`, clean tree. |
| Active change folders | `openspec/changes/issue-9-gateway-firmware-foundation/` (no `state.yaml`, superseded by `openspec/changes/archive/2026-09-29-issue-9-gateway-firmware-foundation/`) and `openspec/changes/iiot-kit-transformation/` (a synced summary of a change that was already archived on 2026-09-27). |
| Toolchain | ESP-IDF v5.4 at `~/esp/esp-idf`, xtensa toolchain present, `cmake` only inside the IDF environment. |
| Board | One ESP32-S3-WROOM-1-N16R8 on `/dev/ttyACM0`. |
| Docker | Available, so the Sparkplug conformance harness can run against a real broker. |

## What each dependent issue delivered

| Issue | Title | Delivered | Runtime or contract |
|---|---|---|---|
| #8 | Hardware support for S3-N16R8 + ST7789V2 | `board_profile`, `board_rgb`, `docs/hardware/esp32-s3-wroom-1-n16r8.md` | runtime |
| #9 | Modular ESP-IDF firmware foundation | `nvs_config` + five component contracts, `TEST_COMPONENTS` registration | mixed |
| #10 | ST7789 + LVGL local screen | `lcd_driver`, `lvgl_port`, `ui_screens`, `assets_brand` contracts | contract |
| #11 | Optional MQTT Sparkplug B | `sparkplug_b` component, nanopb, conformance harness | runtime, disabled by default |
| #12 | Brand profiles without forking | `brand_profile`, `assets_brand`, `tools/brand/build-profile.sh` | runtime |
| #13 | Secure OTA per product and revision | `ota_manifest`, `ota_manager`, `tools/ota/*` | runtime |
| #14 | CI and conformance tests | `.github/workflows/firmware-ci.yml`, host tests, QEMU device tests | runtime |
| #15 | Documentation synced with the S3 | `docs_site/` mirror, `project.md`, `AGENTS.md`, `CLAUDE.md` | documentation |

## Evidence collected for this change

All of it was produced on the working tree of this change, on 2026-10-26,
with the ESP-IDF v5.4 toolchain and the board on `/dev/ttyACM0`.

### Build

| Check | Command | Result |
|---|---|---|
| Gateway build | `idf.py -C firmware/gateway build` | `gateway.bin` 799,504 bytes (0xc3310). Smallest app partition 0x300000; 2,346,224 bytes free; 25.42 % used. |
| On-target test build | `idf.py -C firmware/gateway/test build` | success. |
| Reproducible-build gate | `tools/ci/check-reproducible.sh` | present in CI (`Firmware CI` job `reproducible`), green on `main` (run 40219836073). |
| Secret gate | `tools/ci/check-no-secrets.sh firmware/gateway/build/gateway.bin` | "no secret found". |
| Brand profile build | `tools/brand/build-profile.sh iiot-kit` / `acme-industrial` | both profiles build from the same sources; see `apply-progress.md`. |

### Tests

| Check | Command | Result |
|---|---|---|
| Host unit tests | `cmake -S tools/host-tests -B build/host-tests && ./build/host-tests/iot_host_tests` | **78 Tests 0 Failures 0 Ignored**. |
| Node load | `./build/host-tests/iot_stress --nodes 10 --rounds 50` | 500 frames, 500 accepted, 0 rejected, 0.000 % loss, 0 identity errors, 0 sequence errors, RSS growth 196 KiB. |
| Device tests (logic profile, QEMU) | `tools/ci/run-device-tests.sh firmware/gateway/test build/device-tests 300` | 111 Tests 0 Failures 0 Ignored — **but the application crashed right after the summary**. |
| Device tests (board profile, physical board) | `idf.py -C firmware/gateway/test -p /dev/ttyACM0 flash` + `tools/board/boot_from_flash.py` | 114 Tests 0 Failures 0 Ignored — **but the application crashed right after the summary**. |
| Sparkplug B conformance | `bash tools/sparkplug-conformance/scripts/run-conformance.sh` | **82 checks, 82 passed, 0 failed → PASS** against a real Mosquitto broker. |

### Runtime on the physical board

| Check | Result |
|---|---|
| Boot smoke | `nvs_config` → `board_profile` → `brand_profile` → display contracts → `wifi_manager` (AP `IIoT-Kit-GW`, channel 1, DHCP server on 192.168.4.1) → `mqtt_bridge` → `ota_manager` (running `ota_0`, version 1.0.0, channel stable) → `Returned from app_main()`. |
| Hardware facts | 16 MB flash, 8 MB Octal PSRAM (`Found 8MB PSRAM device`, `SPI SRAM memory test OK`). |
| OTA rollback | Defective image armed in `ota_1`: first boot `Loaded app from partition at offset 0x320000`, the image aborts, the next boot logs `No factory image, trying OTA 0` and loads `0x20000`. `otadata` record 0 reads `seq=2 label='ota_1' state=4 (aborted)` with a valid CRC. |

### Documentation

| Check | Result |
|---|---|
| Strict site build | `mkdocs build --strict` → exit 0, no warnings. |
| Roam links | `python3 tools/ci/check-roamlinks.py` → "OK: every roam link resolves", exit 0. |
| CI | `Firmware CI`, `Docs CI` and `Deploy Tutorial to GitHub Pages` all green on `main` (runs 40219836073, 40219835968, 40219836025). |

## The defect the audit exposed

Both device-test runs ended like this:

```
114 Tests 0 Failures 0 Ignored
OK
I (8943) gateway_test: Main task stack headroom after the run: 0 bytes of 8192
I (8943) main_task: Returned from app_main()
***ERROR*** A stack overflow in task main has been detected.
```

The Unity summary is printed *before* the FreeRTOS overflow detector fires,
so `tools/ci/run-device-tests.sh` — which only looked at the summary and the
verdict — reported success over an application that was crashing. The same
happened in the QEMU profile. The CI on `main` has been green over this
since the suite grew past what the 8192-byte main task stack can hold.

Measured: with `CONFIG_ESP_MAIN_TASK_STACK_SIZE=16384` the same 114 tests
leave **7504 bytes** of headroom and the overflow does not happen.

This is the only defect the audit found that blocks an acceptance criterion:
AC-01 asks for verifiable evidence of *execution*, and an application that
overflows its stack after the tests is not verifiable execution.

## What the audit could not close

Two acceptance criteria of #16 are not satisfiable in this environment, and
no amount of code in this repository changes that:

- **AC-02, "with the verified physical carrier".** The carrier pinout comes
  from a user-supplied image, not from manufacturer documentation. The
  connected board proves the module (16 MB flash, 8 MB Octal PSRAM), not the
  carrier's GPIO assignment.
- **AC-03, "the ST7789V2 and ESP-NOW work".** `lcd_driver`, `lvgl_port`,
  `ui_screens` and `espnow_manager` are contract-only by design. The display
  hold is a project constraint (`openspec/config.yaml` → `open_constraints`):
  no GPIO assignment for the 8-pin module until the carrier and the module
  are physically verified. ESP-NOW is scheduled in a later Fase.

Both were already the agreed scope of #8, #9 and #10 when those issues were
closed as contracts-only. The roll-up inherits that decision; it does not
reopen it.

## Decision

Run the roll-up as a verification-and-honesty change:

1. audit every acceptance criterion against the evidence above;
2. fix the stack-overflow defect and make the CI gate catch its class;
3. write the ledger that maps every criterion to its evidence;
4. close #16 with the ledger, and record the two deferred items as follow-up
   work instead of pretending they are done.
