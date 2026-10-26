# apply-progress — issue #16 gateway product roll-up

## Slices

The change was delivered in one working sequence on `main`, with the
verification runs interleaved. The line budget is far below the 400-line
review budget.

## Slice 1 — The defect

### T-16.01 — The stack of the test application, sized by measurement

- **File changed**: `firmware/gateway/test/sdkconfig.defaults`.
- **Change**: `CONFIG_ESP_MAIN_TASK_STACK_SIZE` 8192 → 16384, with the
  comment that records the history (3584 overflowed in `nvs_flash_init()`,
  8192 stopped being enough when the suite grew) and the measurement (114
  tests, 7504 bytes of headroom), and the note that the generated
  `sdkconfig` has to be removed for the change to take effect.
- **Measurement**: 8192 → headroom 0 and an overflow; 16384 → headroom 7504
  and no overflow.

### T-16.02 — The product reports its own headroom

- **File changed**: `firmware/gateway/main/main.c`.
- **Change**: `uxTaskGetStackHighWaterMark(NULL)` at the end of `app_main()`,
  logged as `Main task stack headroom after the init chain: N bytes of M`,
  with the rationale that makes the value of
  `CONFIG_ESP_MAIN_TASK_STACK_SIZE` a measurement and not a guess.
- **Measurement on the board**: `5648 bytes of 8192`.

### T-16.03 — The gate stops being blind

- **File changed**: `tools/ci/run-device-tests.sh`.
- **Change**: the run fails when the application crashes after the Unity
  summary, and when the headroom is missing or below the floor (1024 bytes,
  overridable with `HEADROOM_FLOOR_BYTES`). The headroom line is published
  in the summary of the job.
- **Negative tests**: a 4096-byte stack fails on the headroom; a 2048-byte
  stack fails on the missing summary; the crash pattern matches the real
  8192 log line `***ERROR*** A stack overflow in task main has been
  detected.`.

### T-16.05 — The suite ends deterministically

- **File changed**: `firmware/gateway/test/main/test_main.c`.
- **Change**: the run ends with `esp_restart()`.
- **Why it was needed**: the CI profile used to end the QEMU run because the
  overflow *crashed* the chip. With the stack fixed, the application returned
  to the idle task and QEMU ran until the timeout, so a passing run was
  reported as a timeout. `esp_restart()` is how ESP-IDF ends a Unity run,
  and it is not a panic: the panic handler prints `***ERROR***` and
  `Rebooting...`, and `esp_restart()` prints neither.

## Slice 2 — The evidence

| Id | Check | Result |
|---|---|---|
| T-16.04 | Build, size report, secret gate | `gateway.bin` 799,504 B, 25.42 % used, no secret in the artefact. |
| T-16.05 | Host unit tests | 78 Tests 0 Failures 0 Ignored. |
| T-16.06 | 10-node stress | 500/500 frames, 0.000 % loss, 0 identity errors, RSS growth 196 KiB. |
| T-16.07 | Device suite, logic profile (QEMU) | 111 Tests 0 Failures, headroom 7504 of 16384, gate exit 0. |
| T-16.08 | Device suite, board profile (board) | 114 Tests 0 Failures, headroom 7504 of 16384, no crash. |
| T-16.09 | Sparkplug B conformance | 82 checks, 82 passed, 0 failed. |
| T-16.10 | Boot smoke | Full init chain, 16 MB flash, 8 MB Octal PSRAM, AP `IIoT-Kit-GW` channel 1, DHCP 192.168.4.1, `ota: running ota_0`, headroom 5648 of 8192. |
| T-16.11 | OTA rollback | Defective image in `ota_1` → aborted → back to `ota_0`; `otadata` record `seq=2 label='ota_1' state=4 (aborted)`, CRC valid. |
| T-16.12 | Brand profiles | `iiot-kit` and `acme-industrial` built from the same sources; different identities, different digests, empty diff over the brand components. |
| T-16.13 | Docs and CI | `mkdocs build --strict` exit 0; roam links OK; three CI workflows green on `main`. |

## Slice 3 — The record

| Id | Change | Result |
|---|---|---|
| T-16.14 | `verify-report.md` | The ledger: one row per acceptance criterion of issue #16. |
| T-16.15 | `docs/testing/hardware-tests.md` | The crash gate, the headroom floor, the stale-`sdkconfig` trap and the deterministic end of the run. |
| T-16.16 | `AGENTS.md`, `CLAUDE.md` | The lesson in the testing instructions and in the list of common mistakes. |
| T-16.17 | `odd/tasks/issue-16-gateway-product-rollup.md` | The work plan, in Spanish, as the other plans of `odd/tasks/`. |
| T-16.18 | `openspec/changes/issue-9-gateway-firmware-foundation/apply-progress.md` | The committed conflict markers removed; the resolution verified against the archived copy, which is byte-identical once the markers are gone. |

## Files changed

| File | Kind |
|---|---|
| `firmware/gateway/test/sdkconfig.defaults` | configuration |
| `firmware/gateway/test/main/test_main.c` | source |
| `firmware/gateway/main/main.c` | source |
| `tools/ci/run-device-tests.sh` | tooling |
| `docs/testing/hardware-tests.md` | documentation |
| `AGENTS.md`, `CLAUDE.md` | documentation |
| `odd/tasks/issue-16-gateway-product-rollup.md` | plan (new) |
| `openspec/changes/issue-16-gateway-product-rollup/**` | change artifacts (new) |
| `openspec/changes/issue-9-gateway-firmware-foundation/apply-progress.md` | repository record |

No partition, protocol, NVS schema or public API changed. No GPIO was
assigned. The hardware hold is untouched.
