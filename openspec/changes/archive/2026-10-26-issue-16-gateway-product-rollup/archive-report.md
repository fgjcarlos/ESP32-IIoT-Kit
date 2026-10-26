# archive-report — issue #16 gateway product roll-up

## Outcome

**COMPLETED, with two criteria partially met and recorded as deferred.**

Issue #16 is the umbrella proposal of the product. Its eight dependent
issues (#8 … #15) were closed before this change and their code is on
`main`. This change audited every acceptance criterion of #16 against real
evidence, fixed the defect the audit exposed, and wrote the ledger.

The ledger is `verify-report.md`. Four of the six criteria are met. The two
that are not are AC-02 (the physical carrier is not verified) and AC-03 (the
ST7789V2 and the ESP-NOW are contract-only under the hardware hold). Both
need the physical unit in the developer's hands; neither can be closed from
a terminal, and neither is presented as done.

## The defect this change fixed

The on-target Unity suite exhausted the 8192-byte main task stack:

```
114 Tests 0 Failures 0 Ignored
OK
I (8943) gateway_test: Main task stack headroom after the run: 0 bytes of 8192
***ERROR*** A stack overflow in task main has been detected.
```

The Unity summary is printed before the FreeRTOS overflow detector speaks,
so `tools/ci/run-device-tests.sh` reported success over an application that
was crashing — in both the QEMU profile and the board profile. The CI on
`main` had been green over this since the suite outgrew the stack.

The fix has three parts:

| Part | Change |
|---|---|
| The value | `CONFIG_ESP_MAIN_TASK_STACK_SIZE` 8192 → 16384 in the test application, measured: 7504 bytes of headroom with the 114 tests of the suite. |
| The end of the run | The suite ends with `esp_restart()`. The CI profile used to end the QEMU run because the overflow *crashed* the chip; with the stack fixed the application idled and the run hit the CI timeout. `esp_restart()` is how ESP-IDF ends a Unity run, and it is not a panic, so the gate still tells a crash from a clean end. |
| The gate | The run fails when the application crashes after the summary, and when the headroom of the main task is missing or below the floor of 1024 bytes. |

The product reports its own headroom too, at the end of the initialisation
chain of `app_main()`: 5648 bytes of 8192 on the board.

## Verification artifacts

| Artifact | Where |
|---|---|
| The ledger, one row per criterion of #16 | `verify-report.md` |
| The exploration, with the evidence inventory | `explore.md` |
| The proposal, with the scope and the rollback plan | `proposal.md` |
| The spec, in Given/When/Then with RFC 2119 keywords | `spec.md` |
| The design, with the analysis of the defect and the memory budget | `design.md` |
| The tasks, and the per-task evidence | `tasks.md`, `apply-progress.md` |
| The work plan, in European Spanish | `odd/tasks/issue-16-gateway-product-rollup.md` |

## Delivery shape

| Field | Value |
|---|---|
| Workflow | ODD, with the SDD record under `openspec/`. |
| Chain strategy | stacked-to-main: one slice per PR, each based on `main` and merged before the next. |
| Review budget | 400 lines per PR. Every slice is under it; the largest is 311. |
| Slice count | 7. |

## Decisions captured

- **The criterion is not the criterion.** AC-02 names "the verified physical
  carrier" and AC-03 names a working ST7789V2 and ESP-NOW. Neither is
  satisfiable without the physical unit, and the issue was written before
  the hardware hold was understood in those terms. The roll-up records them
  as deferred rather than closing them dishonestly.
- **The contracts stay contracts.** No runtime was implemented for
  `espnow_manager`, `http_server`, `mqtt_bridge`, `lcd_driver`, `lvgl_port`,
  `ui_screens` or `display_manager`. The display hold of
  `openspec/config.yaml` → `open_constraints` forbids a GPIO assignment
  until the carrier and the panel are verified, and ESP-NOW belongs to
  Fase 2.
- **A measurement beats a value.** Both stacks are now measured at runtime
  and checked by the gate. A configuration value that nothing checks is a
  guess, and this repository had one.
- **The generated `sdkconfig` is not tracked.** A stale one keeps the old
  value of `CONFIG_ESP_MAIN_TASK_STACK_SIZE`, so the change of the default
  would have had no effect on an existing working tree. The board command
  removes the generated file, and the headroom floor catches the case where
  nobody does.

## Bugs caught and fixed during delivery

- **The stack overflow itself**, and with it the blind spot of the CI gate
  that reported a green summary over a crashing application.
- **The run that could not end.** With the stack fixed, the QEMU profile
  idled until the timeout, because the crash had been the thing that
  restarted the chip. Found by running the gate, not by reading the code.
- **The stale `sdkconfig`**, which kept the old stack on an existing working
  tree and made the first attempt at the fix look like it had no effect.
- **The committed conflict markers** of
  `openspec/changes/issue-9-gateway-firmware-foundation/apply-progress.md`,
  the residue of a stash applied with a conflict. The resolution was taken
  from the archived copy, which is byte-identical once the markers are gone.

## What is NOT in this change

- The runtime of the display, of ESP-NOW and of the HTTP server.
- `firmware/node/` and the Preact SPA. Neither is on disk.
- The physical verification of the carrier and of the panel.
- The STA association against a site network: the local credentials header
  is absent, so the manager reports `STA disabled (no SSID in credentials)`.
  That is the documented CI behaviour, not a defect.
