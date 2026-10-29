# Tasks

## 1. Diagnostic

- [x] T-01.1 ESP-IDF project under `tools/lcd-smoke/`, target `esp32s3` pinned
      in `CMakeLists.txt`, native USB console, no dependency of its own.
- [x] T-01.2 SPI2 bus at 10 MHz, panel IO and ST7789 controller created with
      the official ESP-IDF driver, reset and initialisation done.
- [x] T-01.3 One frame buffer of 108800 bytes in internal memory the DMA can
      read, allocated once and never freed.
- [x] T-01.4 Display enabled, frame drawn once, result reported as queued with
      the visual confirmation pending.
- [x] T-01.5 Kconfig options for the X gap, the Y gap, the inversion and the
      RGB or BGR order.

## 2. Pattern and its tests

- [x] T-02.1 `lcd_smoke_pattern` component: five bands of 64 rows, a two pixel
      border, high byte first, null and size checks.
- [x] T-02.2 Unit tests: every pixel of every band, the border and its
      precedence, the wire bytes, and the rejected buffers.
- [x] T-02.3 `run-host.sh`, a runner that reuses the harness and the shims of
      `tools/host-tests` without modifying them. The same test file is usable
      in an ESP-IDF test project, which the diagnostic application does not
      register.

## 3. Documentation

- [x] T-03.1 `tools/lcd-smoke/README.md` with the exact build, test, flash and
      monitor commands, the port placeholder and the gateway restore.
- [x] T-03.2 OpenSpec artifacts of this change, and the integrator note in
      `docs/hardware/esp32-s3-wroom-1-n16r8.md`, written as a statement and
      not as a measurement.
- [x] T-03.3 Pointer to the diagnostic in `tools/board/README.md`.

## 4. Verification

- [x] T-04.1 `tools/lcd-smoke/run-host.sh`: 6 tests, 0 failures, including independent full-frame/bounds regressions.
- [x] T-04.2 `cmake -S tools/host-tests -B build/host-tests && cmake --build
      build/host-tests && ./build/host-tests/iot_host_tests`: 78 tests, 0
      failures, the suite is unchanged.
- [x] T-04.3 Configure and compile of the diagnostic application with the
      ESP-IDF toolchain, out of the worktree.
- [x] T-04.4 Parent full diagnostic build: `build/lcd-smoke/lcd_smoke.bin`, 246080 bytes; gateway build also passed. No serial device accessed.
- [ ] T-04.5 Flash and look at the panel: an integrator with the board in
      front of them.
