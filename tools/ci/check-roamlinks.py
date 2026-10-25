#!/usr/bin/env python3
"""Check that every Obsidian-style roam link used by the docs site resolves.

The MkDocs site is built from `docs_site/`, whose pages are symlinks into the
canonical sources (`Tutorial/`, `Fases/`, `knowledge/`). The `roamlinks`
plugin rewrites `[[Note]]` into a relative Markdown link by looking for a file
whose simplified name matches the note name inside `docs_dir`.

This script reproduces that resolution offline so a broken roam link is
reported as a normal error instead of a `mkdocs build --strict` warning.

Usage:  python3 tools/ci/check-roamlinks.py [docs_dir]
Exit code 0 when every roam link resolves, 1 otherwise.
"""

from __future__ import annotations

import re
import sys
from pathlib import Path

# Same expression as mkdocs_roamlinks_plugin.plugin.ROAMLINK_RE.
ROAMLINK_RE = re.compile(
    r"\[\[(.*?)(\#.*?)?(?:\|([^\D][^\|\]]+[\d]*))?(?:\|(\d+)(?:x(\d+))?)?\]\]"
)


def simplify(filename: str) -> str:
    """Mirror of RoamLinkReplacer.simplify: drop - _ and spaces, drop .md."""
    return re.sub(r"[\-_ ]", "", filename.lower()).replace(".md", "")


def main() -> int:
    root = Path(__file__).resolve().parents[2]
    docs_dir = Path(sys.argv[1]) if len(sys.argv) > 1 else root / "docs_site"
    if not docs_dir.is_dir():
        print(f"error: docs dir not found: {docs_dir}", file=sys.stderr)
        return 2

    # Index every file name the plugin can see, following the same walk.
    names: dict[str, list[Path]] = {}
    for path in sorted(docs_dir.rglob("*")):
        if path.is_file():
            names.setdefault(simplify(path.name), []).append(path)

    unresolved: list[tuple[Path, str]] = []
    referenced: set[str] = set()

    for page in sorted(docs_dir.rglob("*.md")):
        if not page.is_file():
            continue
        text = page.read_text(encoding="utf-8")
        for match in ROAMLINK_RE.finditer(text):
            target = (match.group(1) or "").strip()
            if not target or "/" in target or target.startswith("http"):
                continue
            key = simplify(target)
            referenced.add(key)
            if key not in names:
                unresolved.append((page, target))

    # A note name that matches more than one page is only a problem when a
    # roam link actually points at it: the plugin silently keeps the last hit.
    for key in sorted(referenced):
        paths = names.get(key, [])
        if len(paths) > 1:
            print(
                f"ambiguous roam target '{key}': "
                + ", ".join(str(p.relative_to(root)) for p in paths)
            )

    for page, target in unresolved:
        rel = page.relative_to(root)
        print(f"unresolved roam link [[{target}]] in {rel}")

    if unresolved:
        print(f"FAIL: {len(unresolved)} unresolved roam link(s)")
        return 1
    print("OK: every roam link resolves")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
