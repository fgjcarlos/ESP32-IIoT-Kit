#!/usr/bin/env python3
"""Build the `otadata` binary that points the bootloader at a slot.

The `otadata` partition holds two 32-byte records, one per 4 KiB sector.
Each record says which slot the bootloader has to start and in which state
that slot is:

    offset  0   uint32  ota_seq      the sequence number of the selection
    offset  4   char[20] seq_label   the label of the slot
    offset 24   uint32  ota_state    new / pending verify / valid / aborted
    offset 28   uint32  crc          CRC-32 of the four bytes of ota_seq

The bootloader maps the sequence number to a slot with
`(ota_seq - 1) % number_of_slots`, so an odd sequence number starts
`ota_0` and an even one starts `ota_1`.

This tool is what makes the rollback demonstration of issue #13
reproducible: it lets anybody arm a slot by hand, without an update that
would have to go through the whole release pipeline.

Usage:
    tools/ota/make-otadata.py --slot ota_1 --out otadata.bin
    tools/ota/make-otadata.py --slot ota_1 --state pending-verify --out otadata.bin

The states are the ones of `esp_ota_img_states_t` in ESP-IDF:

    new            0x0  the bootloader starts it and marks it pending verify
    pending-verify 0x1  it was started and has not confirmed itself yet
    valid          0x2  it confirmed itself
    aborted        0x4  it did not confirm itself and was discarded
"""

import argparse
import binascii
import struct
import sys
from pathlib import Path

SECTOR_SIZE = 4096
PARTITION_SIZE = 0x2000
ENTRY_SIZE = 32
LABEL_SIZE = 20

STATES = {
    "new": 0x0,
    "pending-verify": 0x1,
    "valid": 0x2,
    "invalid": 0x3,
    "aborted": 0x4,
}


def rom_crc32_le(crc: int, data: bytes) -> int:
    """The CRC of the bootloader, `esp_rom_crc32_le`.

    The ROM complements the seed before the loop and complements the result
    afterwards, which is *not* the same as the plain CRC-32 of the data:
    `zlib.crc32(data)` and `esp_rom_crc32_le(0xFFFFFFFF, data)` differ. The
    bootloader compares against the latter, so this is the one to use.
    """
    value = (~crc) & 0xFFFFFFFF
    for byte in data:
        value ^= byte
        for _ in range(8):
            value = (value >> 1) ^ (0xEDB88320 & -(value & 1))
    return (~value) & 0xFFFFFFFF


def build_entry(ota_seq: int, label: str, state: int) -> bytes:
    """Build one 32-byte selection record."""
    encoded_label = label.encode("ascii")[: LABEL_SIZE - 1]
    sequence = struct.pack("<I", ota_seq)
    crc = rom_crc32_le(0xFFFFFFFF, sequence)
    return (
        sequence
        + encoded_label.ljust(LABEL_SIZE, b"\x00")
        + struct.pack("<I", state)
        + struct.pack("<I", crc)
    )


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--slot", required=True, help="the slot to arm: ota_0 or ota_1")
    parser.add_argument("--sequence", type=int, help="the sequence number (odd for ota_0, even for ota_1)")
    parser.add_argument("--state", default="new", choices=sorted(STATES), help="the state of the record")
    parser.add_argument("--out", type=Path, required=True, help="where the binary is written")
    args = parser.parse_args()

    if args.slot not in ("ota_0", "ota_1"):
        sys.exit(f"unknown slot {args.slot}")

    if args.sequence is None:
        # The sequence number has to satisfy (ota_seq - 1) % 2 == the index of
        # the slot: odd for ota_0, even for ota_1.
        args.sequence = 1 if args.slot == "ota_0" else 2

    entry = build_entry(args.sequence, args.slot, STATES[args.state])
    image = entry.ljust(SECTOR_SIZE, b"\xff")
    image += b"\xff" * (PARTITION_SIZE - SECTOR_SIZE)
    args.out.write_bytes(image)

    # Cross-check against the convention of ESP-IDF's own `otatool.py`, which
    # seeds the CRC with 0xFFFFFFFF instead of complementing it.
    expected = binascii.crc32(struct.pack("<I", args.sequence), 0xFFFFFFFF) % (1 << 32)
    actual = struct.unpack("<I", entry[28:32])[0]
    if expected != actual:
        sys.exit(f"the CRC of the record is wrong: {actual:#010x} != {expected:#010x}")

    print(f"otadata for {args.slot}: sequence={args.sequence} state={args.state}")
    print(f"  crc=0x{actual:08x}")
    print(f"  written to {args.out} ({len(image)} bytes)")
    return 0


if __name__ == "__main__":
    sys.exit(main())
