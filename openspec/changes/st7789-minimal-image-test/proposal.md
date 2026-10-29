# ST7789 minimal image test

## Why

The panel of the gateway board lights up but stays black, and no image has
ever reached it. The gateway firmware cannot show whether the SPI link, the
panel settings or the pixel data are wrong, because the gateway does not
drive the panel yet.

## What

A standalone ESP-IDF project, `tools/lcd-smoke`, that drives the panel with
the official ESP-IDF ST7789 support and draws one fixed frame: five
horizontal RGB565 bands with a two pixel border.

The diagnostic is deliberately minimal: one project, one frame, one draw, no
framework around it. It is not a display driver and it does not become part
of the gateway firmware.

## Non-goals

- It does not become a gateway component and it is not registered in the
  gateway test suite.
- It adds no LVGL, no backlight control, no NVS, no networking and no
  dependency of its own.
- It does not drive GPIO7.

## Verification

The pattern is proven by unit tests that run on the host, without a board.
The picture on the panel is not part of this change: it is observed by an
integrator, with the hardware in front of them.

## References

- Hardware profile: `docs/hardware/esp32-s3-wroom-1-n16r8.md`
- Diagnostic: `tools/lcd-smoke/README.md`
- Task: `odd/tasks/st7789-minimal-image-test.md`
