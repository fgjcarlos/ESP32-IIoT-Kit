#!/usr/bin/env python3
"""Generate the embedded portal asset table from the committed web dist.

Reads the fixed manifest of the W02 portal bundle (issue #69) and emits a C
source file with the asset bytes so the firmware serves the portal from
flash without any filesystem. The generator is invoked by the http_server
component build and by the host test build; the generated file is never
committed.

Usage:
    embed_assets.py --dist <dist-dir> --out <portal_assets_data.c>
"""

import argparse
from pathlib import Path

# (dist file, served path, MIME type)
MANIFEST = [
    ("index.html", "/", "text/html"),
    ("app.js", "/app.js", "application/javascript"),
    ("assets/index.css", "/assets/index.css", "text/css"),
]


def c_string_literal(text: str) -> str:
    escaped = text.replace("\\", "\\\\").replace('"', '\\"')
    return f'"{escaped}"'


def c_byte_array(data: bytes) -> str:
    hex_bytes = ", ".join(f"0x{b:02x}" for b in data)
    return f"{{{hex_bytes}}}"


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--dist", required=True, type=Path, help="web dist directory")
    parser.add_argument("--out", required=True, type=Path, help="generated C file")
    args = parser.parse_args()

    entries = []
    for index, (dist_name, path, mime) in enumerate(MANIFEST):
        dist_file = args.dist / dist_name
        data = dist_file.read_bytes()
        entries.append((index, dist_name, path, mime, data))

    lines = [
        "// Generated from firmware/gateway/web/dist by tools/web/embed_assets.py.",
        "// Do not edit: regenerate with the build.",
        "",
        '#include "portal_assets.h"',
        "",
    ]
    for index, dist_name, _path, _mime, data in entries:
        lines.append(f"static const uint8_t k_portal_asset_{index}[] = "
                     f"{c_byte_array(data)};")
        lines.append("")

    lines.append("const portal_asset_t k_portal_assets[] = {")
    for index, dist_name, path, mime, data in entries:
        lines.append(
            f"    {{ {c_string_literal(path)}, {c_string_literal(mime)}, "
            f"sizeof(k_portal_asset_{index}), k_portal_asset_{index} }},"
        )
    lines.append("};")
    lines.append("")
    lines.append("const size_t k_portal_assets_count =")
    lines.append("    sizeof(k_portal_assets) / sizeof(k_portal_assets[0]);")
    lines.append("")

    args.out.parent.mkdir(parents=True, exist_ok=True)
    args.out.write_text("\n".join(lines), encoding="utf-8")
    total = sum(len(data) for *_, data in entries)
    print(f"embedded {len(entries)} portal assets, {total} bytes total")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
