#!/usr/bin/env python3
"""Generate the RGB565 logo of a brand profile.

The logos of `firmware/gateway/components/brand_profile/profiles/<id>/` are
*derived*, not hand-painted: the generator reads the palette of the profile
from the Kconfig and the geometry from the profile manifest, so a logo can
never drift away from the colours the firmware actually uses.

Why a raw RGB565 file instead of a PNG or a JPEG:

  - The panel is RGB565, so the asset is stored in the exact format the
    display consumes: no decoder, no conversion at boot, no extra RAM.
  - The file is deterministic, so the build output does not depend on an
    image toolchain and the flash budget is exactly known.

The output is a headerless stream of little-endian 16-bit pixels, row by row
from the top-left corner.

Usage:

    tools/brand/gen_logo.py \\
        --kconfig firmware/gateway/components/brand_profile/Kconfig.projbuild \\
        --profile iiot-kit \\
        --manifest firmware/gateway/components/brand_profile/profiles/iiot-kit/profile.manifest \\
        --out firmware/gateway/components/brand_profile/profiles/iiot-kit/logo.rgb565
"""

from __future__ import annotations

import argparse
import math
import os
import re
import sys
from typing import Dict, Tuple

# Symbols of the brand Kconfig whose per-profile default the generator needs.
COLOR_SYMBOLS = ("BRAND_COLOR_BACKGROUND", "BRAND_COLOR_FOREGROUND",
                 "BRAND_COLOR_ACCENT")

RGB = Tuple[int, int, int]


def parse_kconfig_palette(kconfig_path: str, profile_choice: str) -> Dict[str, RGB]:
    """Read the per-profile colour defaults of a Kconfig file.

    The Kconfig expresses the per-profile defaults as
    `default "0B1020" if BRAND_PROFILE_IIOT_KIT`, so the profile is selected
    by matching the condition that follows the default value.
    """
    with open(kconfig_path, "r", encoding="utf-8") as handle:
        content = handle.read()

    palette: Dict[str, RGB] = {}
    for symbol in COLOR_SYMBOLS:
        # `default "<value>" if BRAND_PROFILE_<CHOICE>`
        pattern = re.compile(
            r'default\s+"([0-9A-Fa-f]{6})"\s+if\s+BRAND_PROFILE_'
            + re.escape(profile_choice) + r'\b'
        )
        match = pattern.search(content)
        if match is None:
            raise SystemExit(
                "no default for {} in {} for profile {}".format(
                    symbol, kconfig_path, profile_choice))
        palette[symbol] = hex_to_rgb(match.group(1))

    return palette


def parse_manifest(manifest_path: str) -> Dict[str, str]:
    """Read the `key=value` entries of a profile manifest."""
    values: Dict[str, str] = {}
    with open(manifest_path, "r", encoding="utf-8") as handle:
        for line in handle:
            line = line.strip()
            if not line or line.startswith("#"):
                continue
            if "=" not in line:
                raise SystemExit("malformed manifest line: {}".format(line))
            key, value = line.split("=", 1)
            values[key.strip()] = value.strip()
    return values


def hex_to_rgb(value: str) -> RGB:
    value = value.lstrip("#")
    return (int(value[0:2], 16), int(value[2:4], 16), int(value[4:6], 16))


def rgb_to_rgb565(color: RGB) -> int:
    """Pack an 8-bit RGB triplet into RGB565 (5 bits red, 6 green, 5 blue)."""
    red, green, blue = color
    return ((red & 0xF8) << 8) | ((green & 0xFC) << 3) | (blue >> 3)


def in_circle(x: int, y: int, size: int, margin: int) -> bool:
    radius = size / 2.0 - margin
    center = (size - 1) / 2.0
    return math.hypot(x - center, y - center) <= radius


def in_square(x: int, y: int, size: int, margin: int) -> bool:
    center = (size - 1) / 2.0
    half = size / 2.0 - margin
    return abs(x - center) <= half and abs(y - center) <= half


def in_wave(x: int, y: int, size: int) -> bool:
    """A sine wave crossing the badge: the 'IIoT-Kit' mark."""
    period = size / 2.0
    amplitude = size / 8.0
    thickness = max(2, size // 16)
    center = (size - 1) / 2.0
    wave = center + amplitude * math.sin(2.0 * math.pi * x / period)
    return abs(y - wave) <= thickness


def in_chevron(x: int, y: int, size: int) -> bool:
    """A '>' chevron: the 'Acme Industrial' mark."""
    thickness = max(2, size // 12)
    apex_x = size * 0.62
    apex_y = (size - 1) / 2.0
    start_x = size * 0.30
    start_y = size * 0.24
    end_y = size * 0.76

    def distance_to_segment(px, py, ax, ay, bx, by) -> float:
        dx, dy = bx - ax, by - ay
        length_squared = dx * dx + dy * dy
        if length_squared == 0:
            return math.hypot(px - ax, py - ay)
        t = max(0.0, min(1.0, ((px - ax) * dx + (py - ay) * dy) / length_squared))
        return math.hypot(px - (ax + t * dx), py - (ay + t * dy))

    upper = distance_to_segment(x, y, start_x, start_y, apex_x, apex_y)
    lower = distance_to_segment(x, y, apex_x, apex_y, start_x, end_y)
    return min(upper, lower) <= thickness


GLYPHS = {"wave": in_wave, "chevron": in_chevron}


def render(size: int, shape: str, glyph: str,
           background: RGB, accent: RGB, foreground: RGB) -> bytes:
    """Render the logo as a raw RGB565 stream."""
    if shape == "circle":
        shape_test = lambda x, y: in_circle(x, y, size, 2)
    elif shape == "square":
        shape_test = lambda x, y: in_square(x, y, size, 4)
    else:
        raise SystemExit("unknown shape: {}".format(shape))

    glyph_test = GLYPHS.get(glyph)
    if glyph_test is None:
        raise SystemExit("unknown glyph: {}".format(glyph))

    background_565 = rgb_to_rgb565(background)
    accent_565 = rgb_to_rgb565(accent)
    foreground_565 = rgb_to_rgb565(foreground)

    out = bytearray()
    for y in range(size):
        for x in range(size):
            if glyph_test(x, y, size):
                pixel = foreground_565
            elif shape_test(x, y):
                pixel = accent_565
            else:
                pixel = background_565
            out += bytes((pixel & 0xFF, (pixel >> 8) & 0xFF))

    return bytes(out)


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__,
                                     formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument("--kconfig", required=True,
                        help="path to the brand_profile Kconfig.projbuild")
    parser.add_argument("--profile", required=True,
                        help="profile id, for example 'iiot-kit'")
    parser.add_argument("--manifest", required=True,
                        help="path to the profile manifest")
    parser.add_argument("--out", required=True,
                        help="path of the generated RGB565 file")
    args = parser.parse_args()

    # The Kconfig choice symbol is the upper-cased profile id.
    choice = args.profile.upper().replace("-", "_")

    palette = parse_kconfig_palette(args.kconfig, choice)
    manifest = parse_manifest(args.manifest)

    width = int(manifest["logo_width"])
    height = int(manifest["logo_height"])
    if width != height:
        raise SystemExit("only square logos are supported")

    data = render(width,
                  manifest.get("logo_shape", "circle"),
                  manifest.get("logo_glyph", "wave"),
                  palette["BRAND_COLOR_BACKGROUND"],
                  palette["BRAND_COLOR_ACCENT"],
                  palette["BRAND_COLOR_FOREGROUND"])

    expected = width * height * 2
    if len(data) != expected:
        raise SystemExit("rendered {} bytes, expected {}".format(len(data), expected))

    os.makedirs(os.path.dirname(os.path.abspath(args.out)), exist_ok=True)
    with open(args.out, "wb") as handle:
        handle.write(data)

    print("wrote {} ({} bytes, {}x{} RGB565)".format(args.out, len(data), width, height))
    return 0


if __name__ == "__main__":
    sys.exit(main())
