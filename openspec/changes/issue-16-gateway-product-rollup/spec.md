# spec — issue #16 gateway product roll-up

## Requirement 1 — The on-target suite runs inside the main task stack

The test application runs the whole Unity suite inside `app_main()`, which
runs in the main task. The main task stack **MUST** be large enough for the
deepest test of the suite, and the application **MUST** report the headroom
it left.

### Scenario 1.1 — The suite finishes with headroom to spare

- **Given** the test application built with the main task stack of this
  specification,
- **When** the suite runs on the physical gateway,
- **Then** the Unity summary reports zero failures,
- **And** the log line `Main task stack headroom after the run: N bytes of M`
  reports an `N` greater than zero,
- **And** the log contains no `***ERROR***` line and no stack overflow.

### Scenario 1.2 — The stack is sized by a measurement, not by a guess

- **Given** the suite of 114 tests,
- **When** the main task stack is 16384 bytes,
- **Then** the headroom left after the run is 7504 bytes,
- **And** the configuration carries a comment that states that measurement.

## Requirement 2 — The CI gate does not report green over a crash

`tools/ci/run-device-tests.sh` **MUST** fail the run when the application
crashes after the Unity summary, and **MUST** fail the run when the
headroom of the main task is missing or below its floor.

### Scenario 2.1 — A crash after a green summary fails the run

- **Given** a run whose Unity summary reports zero failures,
- **And** whose log contains `***ERROR*** A stack overflow in task main has been detected.`,
- **When** the gate evaluates the run,
- **Then** it exits with a non-zero status,
- **And** it prints the offending lines.

### Scenario 2.2 — A headroom below the floor fails the run

- **Given** a run whose summary reports zero failures and whose log contains
  no crash,
- **And** whose headroom is 512 bytes,
- **When** the gate evaluates the run,
- **Then** it exits with a non-zero status,
- **And** it reports the measured headroom and the floor.

### Scenario 2.3 — A healthy run passes

- **Given** a run whose summary reports zero failures, whose log contains no
  crash and whose headroom is 7504 bytes,
- **When** the gate evaluates the run,
- **Then** it exits with status 0,
- **And** it publishes the summary, the verdict and the headroom line.

## Requirement 3 — The product reports its own stack headroom

`app_main()` of the gateway **MUST** report the headroom of the main task
after the initialisation chain, so the value of
`CONFIG_ESP_MAIN_TASK_STACK_SIZE` is verifiable on the device.

### Scenario 3.1 — The boot log carries the headroom

- **Given** a gateway that boots with the current configuration,
- **When** the initialisation chain finishes,
- **Then** the log contains
  `Main task stack headroom after the init chain: N bytes of M`,
- **And** `N` is greater than zero.

## Requirement 4 — Every acceptance criterion of issue #16 has an evidence

The roll-up **MUST** produce a ledger that maps every acceptance criterion of
issue #16 to the authoritative evidence that proves it. A criterion that is
not proven **MUST** be recorded as such, with the reason, and **MUST NOT** be
presented as met.

### Scenario 4.1 — The ledger covers every criterion

- **Given** the acceptance criteria of issue #16,
- **When** the ledger is written,
- **Then** every criterion appears exactly once,
- **And** each one carries either an evidence reference or an explicit
  statement that it is not met and why.

### Scenario 4.2 — The deferred criteria are not counted as met

- **Given** the criterion that names the verified physical carrier,
- **And** the criterion that names the ST7789V2 and the ESP-NOW as working,
- **When** the ledger is written,
- **Then** both are recorded as deferred under the hardware hold,
- **And** neither is reported as satisfied.

## Requirement 5 — The documentation states the new gate

The hardware test catalogue **MUST** document that a green Unity summary is
not enough, and the list of common mistakes **MUST** carry the lesson.

### Scenario 5.1 — The catalogue documents the crash gate

- **Given** `docs/testing/hardware-tests.md`,
- **When** it is read,
- **Then** it states that the run fails when the application crashes after
  the summary,
- **And** it states that the headroom is checked against a floor.

### Scenario 5.2 — The common mistakes carry the lesson

- **Given** the list of common mistakes in `AGENTS.md`,
- **When** it is read,
- **Then** it carries an entry about the main task stack of the test
  application and about a green summary that hides a crash.
