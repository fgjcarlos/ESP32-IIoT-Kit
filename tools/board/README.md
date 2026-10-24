# tools/board

Helpers for the connected ESP32-S3 board.

## `boot_from_flash.py`

Restarts the board so that it runs the application from its SPI flash, and
captures the console over the native USB port.

```bash
source ~/esp/esp-idf/export.sh
python3 tools/board/boot_from_flash.py            # reset and capture 15 s
python3 tools/board/boot_from_flash.py --seconds 60
python3 tools/board/boot_from_flash.py --no-reset  # only capture
```

### Why the tool exists

`idf.py flash` ends with the reset that esptool performs, and that reset is
**not** the same as the one of the ESP-IDF monitor:

| Reset | DTR | RTS | Effect on the chip |
|---|---|---|---|
| esptool connection (`--before default_reset`) | asserted | pulsed | DTR maps to IO0, so IO0 goes low and the chip is sampled in *Joint Download Boot* |
| esptool after write (`--after hard_reset`) | untouched | pulsed | RTS pulses EN and DTR stays deasserted, so IO0 stays high and the chip boots from flash |
| ESP-IDF monitor | deasserted | pulsed | Same as above: the application boots from flash |

The tool reproduces the second row explicitly, so the board can be restarted
without leaving a host-side downloader session behind. The details and the
measurements are in `docs/hardware/esp32-s3-wroom-1-n16r8.md`.

The chip re-enumerates on the USB bus while it resets, so the tool reopens the
serial device by name before it reads the console.
