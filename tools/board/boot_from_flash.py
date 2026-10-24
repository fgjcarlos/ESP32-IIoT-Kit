#!/usr/bin/env python3
"""Boot the application of the connected ESP32-S3 from its SPI flash.

Why this tool exists
--------------------
``idf.py flash`` finishes with the reset performed by esptool. On the native
USB-Serial/JTAG peripheral of the ESP32-S3 that reset asserts DTR, and the
peripheral maps DTR to IO0, so the chip is sampled in *Joint Download Boot*
(datasheet Table 2-11: GPIO0=0, GPIO46=0) instead of *SPI Boot* (GPIO0=1,
GPIO46=any value). The application is therefore flashed but the unit keeps
landing in the ROM downloader, which looks like "the board does not boot".

The ESP-IDF monitor resets differently: it pulses RTS only (EN) and leaves
DTR deasserted, so IO0 stays high and the chip boots the application from
the SPI flash. This tool reproduces exactly that reset and captures the
console of the board afterwards.

The chip re-enumerates on the USB bus while it resets, so the file descriptor
of the old port becomes stale: the tool reopens the device by name and keeps
reading from it.

Usage
-----
    python3 tools/board/boot_from_flash.py                # boot and capture 15 s
    python3 tools/board/boot_from_flash.py --seconds 60   # capture longer
    python3 tools/board/boot_from_flash.py --no-reset      # capture only

The tool only needs pyserial; it does not talk to the ROM downloader.
"""

from __future__ import annotations

import argparse
import glob
import sys
import threading
import time

import serial

BOOT_MARKERS = (
    "SPI_FAST_FLASH_BOOT",
    "2nd stage bootloader",
    "Project name:",
)


class Console(threading.Thread):
    """Reads the console of the chip once it is running."""

    def __init__(self, port: serial.Serial) -> None:
        super().__init__(daemon=True)
        self.port = port
        self.lines: list[str] = []

    def run(self) -> None:
        buf = b""
        while True:
            try:
                data = self.port.read(4096)
            except Exception:
                return
            if not data:
                time.sleep(0.02)
                continue
            buf += data
            while b"\n" in buf:
                line, buf = buf.split(b"\n", 1)
                text = line.decode("utf-8", "replace").rstrip("\r")
                self.lines.append(text)
                print(f"| {text}", flush=True)


def find_port(explicit: str | None) -> str:
    if explicit:
        return explicit
    ports = sorted(glob.glob("/dev/ttyACM*"))
    if not ports:
        raise SystemExit("no serial port found (expected /dev/ttyACM*)")
    return ports[0]


def wait_for_port(timeout: float = 15.0) -> str:
    """Waits until the USB serial device is enumerable again."""
    deadline = time.monotonic() + timeout
    while time.monotonic() < deadline:
        ports = sorted(glob.glob("/dev/ttyACM*"))
        if ports:
            return ports[0]
        time.sleep(0.1)
    raise SystemExit("the USB serial device did not come back after the reset")


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__.split("\n")[0])
    parser.add_argument("--port", help="serial port (default: the first /dev/ttyACM*)")
    parser.add_argument("--seconds", type=float, default=15.0,
                        help="seconds of console output to capture (default: 15)")
    parser.add_argument("--no-reset", action="store_true",
                        help="do not reset the chip, only capture the console")
    args = parser.parse_args()

    port = find_port(args.port)
    ser = serial.Serial(port, 115200, timeout=0.2)
    # Idle state before the reset: DTR deasserted keeps IO0 high and RTS
    # deasserted keeps EN high, so the chip is neither held in reset nor
    # forced into the downloader.
    ser.setDTR(False)
    ser.setRTS(False)
    console = Console(ser)
    console.start()
    time.sleep(0.3)

    if not args.no_reset:
        # The reset of the ESP-IDF monitor: RTS pulses EN, DTR stays
        # deasserted so IO0 stays high and the chip boots from the flash.
        ser.setDTR(False)
        ser.setRTS(True)
        time.sleep(0.15)
        ser.setRTS(False)
        print("reset issued (EN pulse, IO0 kept high)", flush=True)

        # The chip re-enumerates on the USB bus while it resets, so the file
        # descriptor of the old port is stale: reopen it and keep reading.
        time.sleep(1.0)
        ser.close()
        time.sleep(0.5)
        reopened_path = wait_for_port()
        reopened = serial.Serial(reopened_path, 115200, timeout=0.2)
        # pyserial asserts DTR and RTS when it opens a port: release both,
        # otherwise EN would be held low and the chip would stay in reset.
        reopened.setDTR(False)
        reopened.setRTS(False)
        console = Console(reopened)
        console.start()

    time.sleep(args.seconds)

    text = "\n".join(console.lines)
    booted = any(marker in text for marker in BOOT_MARKERS)
    print(f"\nbooted from the SPI flash: {booted}", flush=True)
    return 0 if booted else 1


if __name__ == "__main__":
    sys.exit(main())
