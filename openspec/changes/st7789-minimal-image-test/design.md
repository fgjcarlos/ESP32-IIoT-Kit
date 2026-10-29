# Design

## Context

A 170x320 ST7789 panel sits on the gateway board and stays black. The
gateway firmware has no display driver yet, so the first question is
narrower than "build a display stack": can the panel be driven at all, and
which settings does it answer to.

## Decisions

**Official ESP-IDF panel support.** The program uses
`esp_lcd_new_panel_io_spi` and `esp_lcd_new_panel_st7789`. A driver of our
own would add code that the ESP-IDF already maintains.

**The pattern lives in its own component.** `lcd_smoke_pattern` fills a frame
buffer and owns nothing else: no handle, no state, no allocation. That keeps
it unit testable on the host and keeps the application readable.

**Fixed geometry.** The pattern is 170x320, the size of the panel it drives.
The public interface is `lcd_smoke_pattern_fill(buffer, buffer_size)`: no
width, no height, no band count to tune. The buffer size and the null check
stay, because the function writes into memory the caller owns.

**One frame buffer, allocated once.** 170 x 320 x 2 = 108800 bytes in
internal memory the DMA can read. It is never freed: the panel IO may still
be sending the frame when the program ends, so the buffer has to outlive
`app_main`.

**The driver splits the transfer.** The SPI bus is opened with the whole
frame as the limit of one transaction. The SPI master reports the size the
chip can really move in one go, and the ESP-IDF panel IO splits the colour
transfer into chunks of that size, keeping the chip select active between
them. The diagnostic does not compute a chunk size of its own.

**No completion callback.** The program does not wait for the transfer and
does not need to: the frame buffer stays valid until the board is reset, so
nothing has to observe the end of the transfer. The program says that the
frame was queued, and the picture is confirmed by a person.

**Provisional settings.** The X gap of 35 columns, the Y gap of 0, the colour
inversion and the RGB or BGR order are software defaults, exposed as Kconfig
options. The SPI mode 0 and the active low reset are assumptions written in
the code. None of them is a measured fact about the panel in hand; finding
out which ones the panel answers to is the purpose of the diagnostic.

**GPIO7 is left alone.** The backlight is wired by the integrator. The
program does not configure it.

## Memory

| Item | Bytes |
|---|---|
| Frame buffer (internal, DMA capable) | 108800 |
| Transaction queue (4 entries) | driver side |

The frame buffer is the price of the diagnostic: a full frame in internal
memory instead of a partial one.

## Verification status

| Check | Result |
|---|---|
| Host unit tests of the pattern | Parent observed 6/6, including the verifier's full-frame/bounds regression |
| Existing host suite of the repository | 78 tests, 0 failures, unchanged |
| ESP-IDF configure and application compilation | passed, out of the worktree |
| Full image build of the diagnostic | passed, `build/lcd-smoke/lcd_smoke.bin` (246080 bytes); gateway build also passed |
| Picture on the panel | integrator confirmed expected pattern; see the canonical hardware profile's standalone functional observation |

## Risks

- A wrong provisional setting gives a wrong picture, not a verdict about the
  panel.
- A correct picture is evidence that the panel accepts RGB565 pixel data. It
  is not evidence of the whole wiring, of the colour order or of the geometry
  of the panel.
