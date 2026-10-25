#!/usr/bin/env python3
"""Report the size of the firmware binaries and the headroom of the partitions.

The ESP-IDF build already refuses to link an image that does not fit in the
partition table (``components/partition_table/check_sizes.py``), so a build
that ends green never overflows a partition. What the build does **not** do
is tell the reader how close the image is to the limit, and it says nothing
about the partitions that are not used by the application (the SPIFFS budget
of the embedded dashboard, for instance).

This tool turns the artefacts of a build into that report:

* the size of the application binary against the smallest application
  partition, which is the limit the bootloader enforces;
* the whole partition table, with the free space of every partition;
* the budget of the embedded SPA, when the dashboard has been built.

It exits with 1 when the image does not fit or when the headroom is below the
configured threshold, so the CI fails on a firmware that is about to overflow
instead of on one that already did.

Usage:
    tools/ci/report-size.py --project-dir firmware/gateway
    tools/ci/report-size.py --project-dir firmware/gateway --min-free-percent 15
"""

from __future__ import annotations

import argparse
import importlib.util
import json
import os
import sys
from pathlib import Path
from typing import Any

# The smallest application partition is the limit the bootloader enforces: an
# image is flashed into the smallest app partition it fits in.
DEFAULT_MIN_FREE_PERCENT = 10.0

# The budget of the embedded Preact SPA, from the architecture decision in
# project.md. The SPA is not built yet; when it is, the tool checks it.
SPA_GZIPPED_BUDGET_BYTES = 200 * 1024


def log(message: str) -> None:
    print(message, file=sys.stderr)


def load_gen_esp32part() -> Any | None:
    """Load the ESP-IDF partition tool, so the report uses the same parser.

    The binary partition table is parsed with the code of ESP-IDF itself
    instead of a second implementation, so the report can never disagree with
    the bootloader about what the table says.
    """
    idf_path = os.environ.get("IDF_PATH")
    if not idf_path:
        return None

    module_path = Path(idf_path) / "components" / "partition_table" / "gen_esp32part.py"
    if not module_path.is_file():
        return None

    spec = importlib.util.spec_from_file_location("gen_esp32part", module_path)
    if spec is None or spec.loader is None:
        return None

    module = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(module)
    return module


def parse_partitions_csv(csv_path: Path) -> list[dict[str, Any]]:
    """Parse the partition CSV of the project.

    This is the fallback for a checkout without ESP-IDF. It understands the
    subset of the format this repository uses: name, type, subtype, offset,
    size and flags.
    """
    partitions: list[dict[str, Any]] = []
    for line_no, raw_line in enumerate(csv_path.read_text().splitlines(), start=1):
        line = raw_line.strip()
        if not line or line.startswith("#"):
            continue

        fields = [field.strip() for field in line.split(",")]
        if len(fields) < 5:
            raise ValueError(f"{csv_path}:{line_no}: a partition needs at least five fields")

        name, ptype, subtype, offset, size = fields[:5]
        try:
            partitions.append(
                {
                    "name": name,
                    "type": ptype,
                    "subtype": subtype,
                    "offset": int(offset, 0),
                    "size": int(size, 0),
                }
            )
        except ValueError as err:
            raise ValueError(f"{csv_path}:{line_no}: {err}") from err

    return partitions


# The integer types of the binary partition table, and the readable names of
# the subtypes this project uses. The binary table is the one the bootloader
# reads, so it is the one the report is built from.
PARTITION_TYPES = {0x00: "app", 0x01: "data"}
DATA_SUBTYPES = {
    0x00: "otadata",
    0x01: "phy_init",
    0x02: "nvs",
    0x03: "coredump",
    0x04: "nvs_keys",
    0x82: "spiffs",
}


def partition_type_name(ptype: int) -> str:
    """Return the readable name of an integer partition type."""
    return PARTITION_TYPES.get(ptype, f"0x{ptype:02x}")


def partition_subtype_name(ptype: int, subtype: int) -> str:
    """Return the readable name of an integer partition subtype."""
    if ptype == 0x00:
        return f"ota_{subtype - 0x10}" if subtype >= 0x10 else "factory"
    return DATA_SUBTYPES.get(subtype, f"0x{subtype:02x}")


def read_partitions(project_dir: Path, build_dir: Path) -> list[dict[str, Any]]:
    """Read the partition table of the build, or of the project."""
    binary_table = build_dir / "partition_table" / "partition-table.bin"
    csv_table = project_dir / "partitions.csv"

    module = load_gen_esp32part()
    if module is not None:
        source = binary_table if binary_table.is_file() else csv_table
        if source.is_file():
            # from_file() returns (table, was_binary).
            table, was_binary = module.PartitionTable.from_file(source.open("rb"))
            if was_binary:
                return [
                    {
                        "name": entry.name,
                        "type": partition_type_name(entry.type),
                        "subtype": partition_subtype_name(entry.type, entry.subtype),
                        "offset": entry.offset,
                        "size": entry.size,
                    }
                    for entry in table
                ]
            # The CSV already carries readable names.
            return [
                {
                    "name": entry.name,
                    "type": entry.type,
                    "subtype": entry.subtype,
                    "offset": entry.offset,
                    "size": entry.size,
                }
                for entry in table
            ]

    if csv_table.is_file():
        return parse_partitions_csv(csv_table)

    raise FileNotFoundError(
        f"No partition table found: neither {binary_table} nor {csv_table} exists"
    )


def find_app_binary(build_dir: Path) -> Path | None:
    """Return the application binary of the build."""
    candidates = sorted(build_dir.glob("*.bin"))
    # The bootloader and the partition table are not application images.
    app_candidates = [
        candidate
        for candidate in candidates
        if candidate.name not in {"bootloader.bin", "partition-table.bin", "ota_data_initial.bin"}
    ]
    if not app_candidates:
        return None
    return app_candidates[0]


def find_spa_image(project_dir: Path, build_dir: Path) -> Path | None:
    """Return the image of the embedded SPA, when the dashboard has been built."""
    for candidate in (
        build_dir / "spiffs.bin",
        build_dir / "web.bin",
        project_dir / "web" / "build" / "spiffs.bin",
    ):
        if candidate.is_file():
            return candidate
    return None


def human_bytes(value: int) -> str:
    """Format a byte count the way the ESP-IDF size report does."""
    if value >= 1024 * 1024:
        return f"{value / (1024 * 1024):.2f} MiB"
    if value >= 1024:
        return f"{value / 1024:.2f} KiB"
    return f"{value} B"


def build_report(
    project_dir: Path,
    build_dir: Path,
    min_free_percent: float,
) -> dict[str, Any]:
    """Collect the sizes and the partition table into a report."""
    partitions = read_partitions(project_dir, build_dir)
    app_partitions = [entry for entry in partitions if entry["type"] == "app"]

    if not app_partitions:
        raise ValueError("The partition table has no application partition")

    app_limit = min(entry["size"] for entry in app_partitions)
    app_binary = find_app_binary(build_dir)

    report: dict[str, Any] = {
        "project_dir": str(project_dir),
        "build_dir": str(build_dir),
        "min_free_percent": min_free_percent,
        "partitions": partitions,
        "app_partition_limit_bytes": app_limit,
        "app_binary": None,
        "app_binary_bytes": None,
        "free_bytes": None,
        "used_percent": None,
        "fits": None,
        "spa": None,
        "failures": [],
    }

    if app_binary is None:
        report["failures"].append("no application binary found in the build directory")
        return report

    app_size = app_binary.stat().st_size
    free = app_limit - app_size
    used_percent = (app_size / app_limit * 100.0) if app_limit else 100.0

    report["app_binary"] = str(app_binary)
    report["app_binary_bytes"] = app_size
    report["free_bytes"] = free
    report["used_percent"] = round(used_percent, 2)

    if free < 0:
        report["fits"] = False
        report["failures"].append(
            f"the application binary needs {human_bytes(app_size)} but the smallest "
            f"application partition is {human_bytes(app_limit)}"
        )
    else:
        report["fits"] = True
        free_percent = (free / app_limit * 100.0) if app_limit else 0.0
        if free_percent < min_free_percent:
            report["failures"].append(
                f"only {free_percent:.2f}% of the application partition is free, "
                f"below the {min_free_percent:.2f}% threshold"
            )

    spa_image = find_spa_image(project_dir, build_dir)
    report["spa"] = {
        "image": str(spa_image) if spa_image else None,
        "size_bytes": spa_image.stat().st_size if spa_image else None,
        "budget_bytes": SPA_GZIPPED_BUDGET_BYTES,
    }
    if spa_image is not None and spa_image.stat().st_size > SPA_GZIPPED_BUDGET_BYTES:
        report["failures"].append(
            f"the embedded SPA is {human_bytes(spa_image.stat().st_size)}, above the "
            f"{human_bytes(SPA_GZIPPED_BUDGET_BYTES)} budget"
        )

    return report


def render_markdown(report: dict[str, Any]) -> str:
    """Render the report as the Markdown block of a CI job summary."""
    lines: list[str] = ["## Firmware size", ""]

    if report["app_binary"] is None:
        lines.append("**FAIL** — no application binary found in the build directory.")
        return "\n".join(lines) + "\n"

    app_size = report["app_binary_bytes"]
    limit = report["app_partition_limit_bytes"]
    free = report["free_bytes"]

    lines += [
        "| Metric | Value |",
        "|---|---|",
        f"| Application binary | `{Path(report['app_binary']).name}` |",
        f"| Size | {human_bytes(app_size)} ({app_size} B) |",
        f"| Smallest application partition | {human_bytes(limit)} ({limit} B) |",
        f"| Free | {human_bytes(free)} ({free} B) |",
        f"| Used | {report['used_percent']}% |",
        f"| Threshold | at least {report['min_free_percent']}% free |",
        "",
    ]

    if report["fits"]:
        lines.append("The application binary fits in its partition.")
    else:
        lines.append("**FAIL** — the application binary does not fit in its partition.")

    lines += ["", "### Partition table", "", "| # | Name | Type | Subtype | Offset | Size |", "|---|---|---|---|---|---|"]
    for index, entry in enumerate(report["partitions"]):
        lines.append(
            f"| {index} | {entry['name']} | {entry['type']} | {entry['subtype']} | "
            f"0x{entry['offset']:x} | {human_bytes(entry['size'])} |"
        )

    spa = report["spa"]
    lines += ["", "### Embedded SPA", ""]
    if spa["image"] is None:
        lines.append(
            "The dashboard has not been built yet, so the SPIFFS budget is not checked. "
            f"The budget is {human_bytes(spa['budget_bytes'])} gzipped."
        )
    else:
        lines.append(
            f"`{Path(spa['image']).name}` is {human_bytes(spa['size_bytes'])} against a "
            f"budget of {human_bytes(spa['budget_bytes'])} gzipped."
        )

    if report["failures"]:
        lines += ["", "### Failures", ""]
        lines += [f"- {failure}" for failure in report["failures"]]

    return "\n".join(lines) + "\n"


def main(argv: list[str] | None = None) -> int:
    parser = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    repo_root = Path(__file__).resolve().parents[2]
    parser.add_argument(
        "--project-dir",
        type=Path,
        default=repo_root / "firmware" / "gateway",
        help="Directory of the ESP-IDF project (default: firmware/gateway)",
    )
    parser.add_argument(
        "--build-dir",
        type=Path,
        default=None,
        help="Directory of the build (default: <project-dir>/build)",
    )
    parser.add_argument(
        "--min-free-percent",
        type=float,
        default=DEFAULT_MIN_FREE_PERCENT,
        help=f"Free space required in the application partition (default: {DEFAULT_MIN_FREE_PERCENT})",
    )
    parser.add_argument("--markdown", type=Path, default=None, help="Write the Markdown report to this file")
    parser.add_argument("--json", type=Path, default=None, help="Write the machine readable report to this file")
    args = parser.parse_args(argv)

    project_dir = args.project_dir.resolve()
    build_dir = (args.build_dir or project_dir / "build").resolve()

    try:
        report = build_report(project_dir, build_dir, args.min_free_percent)
    except (FileNotFoundError, ValueError) as err:
        log(f"report-size: {err}")
        return 1

    markdown = render_markdown(report)
    print(markdown)

    if args.markdown:
        args.markdown.parent.mkdir(parents=True, exist_ok=True)
        args.markdown.write_text(markdown)
    if args.json:
        args.json.parent.mkdir(parents=True, exist_ok=True)
        args.json.write_text(json.dumps(report, indent=2))

    if report["failures"]:
        for failure in report["failures"]:
            log(f"report-size: {failure}")
        return 1

    return 0


if __name__ == "__main__":
    sys.exit(main())
