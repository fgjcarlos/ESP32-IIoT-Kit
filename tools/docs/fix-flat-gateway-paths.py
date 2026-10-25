#!/usr/bin/env python3
"""Rewrite the flat `firmware/gateway/main/<component>.c` paths in the phase and
tutorial docs to the real modular component layout.

The gateway firmware is one ESP-IDF component per subsystem:

    firmware/gateway/components/<name>/<name>.c
    firmware/gateway/components/<name>/include/<name>.h

The files that genuinely live in `main/` are left alone: `main.c`,
`nvs_config.c/.h`, `CMakeLists.txt`, `Kconfig.projbuild`, `idf_component.yml`
and `.gitkeep` are the project skeleton and are still there.

Usage: python3 tools/docs/fix-flat-gateway-paths.py <file> [<file> ...]
"""

from __future__ import annotations

import re
import sys
from pathlib import Path

# Files that legitimately stay in firmware/gateway/main/.
KEEP_IN_MAIN = {
    "main.c",
    "nvs_config.c",
    "nvs_config.h",
    "CMakeLists.txt",
    "Kconfig.projbuild",
    "idf_component.yml",
    ".gitkeep",
}

COMPONENT_RE = re.compile(
    r"firmware/gateway/main/(?P<name>[a-z_][a-z0-9_]*)\.c/\.h"
)
SOURCE_RE = re.compile(r"firmware/gateway/main/(?P<name>[a-z_][a-z0-9_]*)\.c")
HEADER_RE = re.compile(r"firmware/gateway/main/(?P<name>[a-z_][a-z0-9_]*)\.h")


def component_paths(name: str) -> tuple[str, str]:
    base = f"firmware/gateway/components/{name}"
    return f"{base}/{name}.c", f"{base}/include/{name}.h"


def rewrite(text: str) -> tuple[str, int]:
    count = 0

    def sub_pair(match: re.Match[str]) -> str:
        nonlocal count
        name = match.group("name")
        if f"{name}.c" in KEEP_IN_MAIN or f"{name}.h" in KEEP_IN_MAIN:
            return match.group(0)
        count += 1
        source, header = component_paths(name)
        return f"`{source}`, `{header}`"

    # The combined "X.c/.h" shorthand: expand it into the two real paths.
    text = re.sub(
        r"`firmware/gateway/main/(?P<name>[a-z_][a-z0-9_]*)\.c/\.h`",
        sub_pair,
        text,
    )

    def sub_source(match: re.Match[str]) -> str:
        nonlocal count
        name = match.group("name")
        if f"{name}.c" in KEEP_IN_MAIN:
            return match.group(0)
        count += 1
        return f"firmware/gateway/components/{name}/{name}.c"

    def sub_header(match: re.Match[str]) -> str:
        nonlocal count
        name = match.group("name")
        if f"{name}.h" in KEEP_IN_MAIN:
            return match.group(0)
        count += 1
        return f"firmware/gateway/components/{name}/include/{name}.h"

    text = SOURCE_RE.sub(sub_source, text)
    text = HEADER_RE.sub(sub_header, text)
    return text, count


def main() -> int:
    if len(sys.argv) < 2:
        print(__doc__, file=sys.stderr)
        return 2
    total = 0
    for arg in sys.argv[1:]:
        path = Path(arg)
        original = path.read_text(encoding="utf-8")
        updated, count = rewrite(original)
        if updated != original:
            path.write_text(updated, encoding="utf-8")
        print(f"{path}: {count} path(s) rewritten")
        total += count
    print(f"total: {total}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
