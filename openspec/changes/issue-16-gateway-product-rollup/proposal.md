# proposal — issue #16 gateway product roll-up

## Outcome

Close the umbrella proposal of the product with a **ledger**: for every
acceptance criterion of issue #16, the authoritative evidence that proves it
or the honest statement that it is not proven yet.

The change ships three things:

1. **A fix for the defect the audit exposed.** The on-target Unity suite
   exhausted the 8192-byte main task stack. The suite printed
   `114 Tests 0 Failures`, the FreeRTOS overflow detector fired right after,
   and the CI reported success over a crashing application. The stack is
   raised to the measured value and the CI gate now fails on a crash and on
   a headroom below its floor.
2. **A measurement in the product.** `app_main()` now reports the headroom
   of the main task at the end of the initialisation chain, so the value of
   `CONFIG_ESP_MAIN_TASK_STACK_SIZE` is a measurement and not a guess.
3. **The ledger itself** (`verify-report.md`), plus the issue body checked
   off and closed.

## Why this and not more

Issue #16 is not a subsystem: its eight dependent issues are closed and
their code is on `main`. What was missing was the roll-up — the evidence
that the product as a whole satisfies the criteria the issue declares, and
an honest account of the two criteria that depend on physical verification
nobody can do from a terminal.

Implementing the missing runtime (ESP-NOW bring-up, `esp_lcd_*`, LVGL
rendering) is **out of scope** for two independent reasons:

- the display is under an explicit project constraint
  (`openspec/config.yaml` → `open_constraints`): no GPIO assignment for the
  8-pin module until the carrier and the panel are physically verified;
- ESP-NOW is scheduled in Fase 2. The phases are sequential, and this
  repository is in Fase 0.

## Scope

### In scope

- `firmware/gateway/test/sdkconfig.defaults` — the measured main task stack.
- `firmware/gateway/main/main.c` — the headroom measurement of the product.
- `tools/ci/run-device-tests.sh` — the gate that stops a green summary from
  hiding a crash, and the headroom floor.
- `docs/testing/hardware-tests.md` — the new gate, in the catalogue of the
  tests that need a board.
- `AGENTS.md` — the lesson in the list of the mistakes to watch for.
- `openspec/changes/issue-16-gateway-product-rollup/**` — this change.
- `odd/tasks/issue-16-gateway-product-rollup.md` — the work plan.
- The body of issue #16 and its closing comment.

### Out of scope

- Any runtime implementation of `espnow_manager`, `http_server`,
  `mqtt_bridge`, `lcd_driver`, `lvgl_port`, `ui_screens` or
  `display_manager`.
- Any GPIO assignment for the display module or the carrier.
- The `firmware/node/` project and the Preact SPA. Neither is on disk; both
  belong to later Fases.
- The rollback of the two deferred criteria. They are recorded as deferred,
  not as failed.

## Affected projects and their test commands

| Project | Command |
|---|---|
| `firmware/gateway` | `idf.py -C firmware/gateway build` (no test project of its own) |
| `firmware/gateway/test` | `idf.py -C firmware/gateway/test build` + `tools/ci/run-device-tests.sh firmware/gateway/test build/device-tests 300` + the board profile on `/dev/ttyACM0` |
| `tools/host-tests` | `cmake -S tools/host-tests -B build/host-tests && cmake --build build/host-tests -j && ./build/host-tests/iot_host_tests` |
| `docs_site` | `mkdocs build --strict` |

## Rollback plan

- The firmware change is a single configuration value and a log line. To
  roll it back: restore `CONFIG_ESP_MAIN_TASK_STACK_SIZE=8192` in
  `firmware/gateway/test/sdkconfig.defaults` and drop the measurement block
  from `firmware/gateway/main/main.c`. The product keeps working; only the
  evidence disappears.
- The CI gate change is additive: the new checks run after the existing
  ones and only add failure conditions. Removing them restores the previous
  behaviour.
- The documentation change is additive.
- No partition layout, no protocol, no NVS schema and no public API change.
  Nothing in this change can strand a unit on a device.
- This change does not touch any in-flight change artifact. The two active
  folders of `openspec/changes/` are read-only references here; see
  `design.md` for what was found in them.

## Risk

| Risk | Mitigation |
|---|---|
| The larger main task stack costs internal RAM | 16 KiB out of 331 KiB of heap at boot. The measured headroom of the product is reported at runtime. |
| The headroom floor is too tight and breaks the CI on a harmless growth | The floor is 1024 bytes against a measured 7504. The threshold is an environment variable. |
| The crash detector matches a benign line in the log | The patterns are the exact strings FreeRTOS and the ESP-IDF panic handler print. A false positive fails loudly and is one `grep` away from being fixed. |
