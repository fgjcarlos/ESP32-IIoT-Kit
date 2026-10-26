# tasks — issue #16 gateway product roll-up

**Change**: issue-16-gateway-product-rollup
**Date**: 2026-10-26
**Artifact store**: openspec
**Delivery strategy**: auto
**Chain strategy**: stacked-to-main
**Review budget**: 400 changed lines
**Educational phase**: Fase 0 — the roll-up of Fase 0/1 work; no new Fase feature is implemented.

## Guard rails

- No GPIO assignment for the display module or the carrier. The hardware hold
  of `openspec/config.yaml` → `open_constraints` applies unchanged.
- The withdrawn `0.1155 × 0.1155 mm` pixel pitch is not used, and no geometry
  is derived from it.
- No runtime implementation of `espnow_manager`, `http_server`,
  `mqtt_bridge`, `lcd_driver`, `lvgl_port`, `ui_screens` or
  `display_manager`.
- No change to the partition layout, the wire protocol, the NVS schema or any
  public API.
- `firmware/node/**` and `firmware/gateway/web/**` are not on disk and are not
  created by this change.
- The archived change folders are read-only. The only edit to an existing
  change artifact is the removal of the committed conflict markers of
  `openspec/changes/issue-9-gateway-firmware-foundation/apply-progress.md`,
  whose resolution is verified against the archived copy.

---

## Fase 0 — The defect

### T-16.01 — Size the main task stack of the test application by measurement

- [x] **T-16.01** — `firmware/gateway/test/sdkconfig.defaults`:
  `CONFIG_ESP_MAIN_TASK_STACK_SIZE` 8192 → 16384, with the comment that
  records the history (3584 overflowed in `nvs_flash_init()`, 8192 stopped
  being enough when the suite grew) and the measurement (114 tests, 7504
  bytes of headroom).

**Check**: `idf.py -C firmware/gateway/test build`, then the board profile on
`/dev/ttyACM0`. The log must report a headroom greater than zero and no
stack overflow.

### T-16.02 — Report the headroom of the product

- [x] **T-16.02** — `firmware/gateway/main/main.c`: report the headroom of
  the main task at the end of the initialisation chain, with the rationale
  that makes the value of `CONFIG_ESP_MAIN_TASK_STACK_SIZE` a measurement
  instead of a guess.

**Check**: `idf.py -C firmware/gateway build`, flash, boot. The log must
carry `Main task stack headroom after the init chain: N bytes of M` with
`N` greater than zero.

### T-16.03 — Close the hole in the CI gate

- [x] **T-16.03** — `tools/ci/run-device-tests.sh`: fail the run when the
  application crashes after the Unity summary, and when the headroom is
  missing or below the floor (1024 bytes, overridable with
  `HEADROOM_FLOOR_BYTES`).

**Check**: a negative test — a build whose stack is too small must make the
gate exit non-zero, and a healthy build must make it exit zero.

---

## Fase 0 — The evidence

### T-16.04 — Build and measure

- [x] **T-16.04** — Build the gateway and the test application; produce the
  size report; run the secret gate; run the reproducible-build gate.
- [x] **T-16.05** — Run the host unit tests and the 10-node stress harness.
- [x] **T-16.06** — Run the on-target Unity suite in both profiles: the
  logic profile under QEMU and the board profile on the physical gateway.
- [x] **T-16.07** — Run the Sparkplug B conformance harness against a real
  Mosquitto broker.
- [x] **T-16.08** — Boot smoke on the physical board: the whole
  initialisation chain, the hardware facts, the AP.
- [x] **T-16.09** — Reproduce the OTA rollback on the board: arm a
  defective image in the inactive slot, watch it fail, and read back the
  `otadata` record.
- [x] **T-16.10** — Build both brand profiles from the same sources and
  compare the identities and the digests.
- [x] **T-16.11** — Strict site build, roam-link check, and the state of
  the CI on `main`.

---

## Fase 0 — The record

### T-16.12 — Write the ledger

- [x] **T-16.12** — `verify-report.md`: one row per acceptance criterion of
  issue #16, each with the evidence that proves it or the explicit statement
  that it is deferred and why.

### T-16.13 — Document the gate

- [x] **T-16.13** — `docs/testing/hardware-tests.md`: the crash gate and
  the headroom floor in the catalogue of the tests that need a board.
- [x] **T-16.14** — `AGENTS.md`: the lesson in the list of common mistakes.

### T-16.15 — Repair the repository record

- [x] **T-16.15** — Remove the committed conflict markers of
  `openspec/changes/issue-9-gateway-firmware-foundation/apply-progress.md`,
  verified against the archived copy.

### T-16.16 — Close the issue

- [x] **T-16.16** — Check off the eight dependent issues and the acceptance
  criteria in the body of issue #16, publish the ledger as the closing
  comment, and close the issue.

---

## Acceptance summary of this change

| Criterion of this change | Evidence |
|---|---|
| The suite finishes with headroom to spare | 114 Tests 0 Failures; headroom 7504 of 16384; no overflow on the board. |
| The CI gate does not report green over a crash | Negative test: a 4096-byte stack makes the gate exit non-zero on the same run that reported `114 Tests 0 Failures`. |
| The product reports its own headroom | The boot log carries the headroom line with a value greater than zero. |
| Every criterion of issue #16 has an evidence | `verify-report.md`. |
| The documentation states the new gate | `docs/testing/hardware-tests.md` and `AGENTS.md`. |
