# ST7789 image diagnostic

A standalone ESP-IDF program that draws one fixed test frame on the 170x320
ST7789 panel of the gateway board and stops there.

It is a separate project: it shares no code with the gateway firmware and it
does not change any gateway component.

## What the frame shows

The frame is 170x320 RGB565:

- five horizontal bands of 64 rows, from top to bottom: red, green, blue,
  white, black;
- a two pixel magenta border over the outer edge.

The program reports that the frame was queued for the panel. It never reports
that the panel shows it: the picture is confirmed by a person looking at the
board.

What a correct picture is evidence of: the SPI link reaches the controller and
the controller accepts commands and pixel data. What it is not evidence of:
the whole wiring, the colour order or the geometry of the panel. Those still
need the physical check.

## Wiring

The map of the board is in the
[canonical hardware profile](../../docs/hardware/esp32-s3-wroom-1-n16r8.md).
This program drives SPI2 at 10 MHz with the pins the profile lists for the
panel, and it leaves GPIO7 alone.

## Requirements

- ESP-IDF v5.4: `source $HOME/esp/esp-idf/export.sh`
- the board wired as described in the hardware profile

## Build

```sh
source $HOME/esp/esp-idf/export.sh
REPO_ROOT="$(git rev-parse --show-toplevel)"
idf.py -C "$REPO_ROOT/tools/lcd-smoke" -B "$REPO_ROOT/build/lcd-smoke" build
```

The target is already pinned to `esp32s3` in `CMakeLists.txt`, so no
`set-target` step is needed.

## Host test, without a board

```sh
tools/lcd-smoke/run-host.sh
```

It builds the pattern component and its test file with `-std=c11 -Wall
-Wextra -Werror`, links them against the shared host harness of
`tools/host-tests` and the ESP-IDF Unity core, and runs them. The harness and
Unity are built as `-std=gnu11` because they are POSIX code that does not
belong to this project.

## Flash and monitor

The port has to be chosen by hand, this project never guesses it:

```sh
REPO_ROOT="$(git rev-parse --show-toplevel)"
PORT=/dev/ttyACM0            # example only: select the board's actual port
idf.py -C "$REPO_ROOT/tools/lcd-smoke" -B "$REPO_ROOT/build/lcd-smoke" -p "$PORT" flash
idf.py -C "$REPO_ROOT/tools/lcd-smoke" -B "$REPO_ROOT/build/lcd-smoke" -p "$PORT" monitor
```

**Flashing replaces the bootloader, partition table and application**, not just
the running app. The diagnostic's default layout uses a 2 MB flash configuration
and starts its app at `0x10000`, overlapping the gateway's OTA metadata and PHY
storage. Do not expect OTA state to survive. This is not a full-chip erase and
is not evidence of NVS loss or preservation. To restore the gateway's layout
and firmware, use its own project (commands work from either project directory):

```sh
REPO_ROOT="$(git rev-parse --show-toplevel)"
idf.py -C "$REPO_ROOT/firmware/gateway" build
idf.py -C "$REPO_ROOT/firmware/gateway" -p "$PORT" flash
idf.py -C "$REPO_ROOT/firmware/gateway" -p "$PORT" monitor
```

## Provisional tunings

| Setting | Default | Meaning |
|---|---|---|
| `CONFIG_LCD_SMOKE_X_GAP` | 35 | column offset of the visible area inside the controller RAM |
| `CONFIG_LCD_SMOKE_Y_GAP` | 0 | row offset of the visible area |
| `CONFIG_LCD_SMOKE_INVERT_COLOR` | `y` | sends `INVON` after the initialisation |
| `CONFIG_LCD_SMOKE_COLOR_ORDER` | `RGB` | the RGB or BGR bit of `MADCTL` |

These are software defaults, not measured facts about the panel in hand. The
diagnostic exists to find out which ones the panel answers to. The SPI mode 0
and the active low reset are assumptions in the same line.

## Memory

The frame buffer is 170 x 320 x 2 = 108800 bytes. It is allocated once in
internal memory the DMA can read, and it is never freed: the panel IO may
still be sending the frame when the program ends.
