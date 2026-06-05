#!/usr/bin/env python3
"""Collect W18 paper reproduction kernel launch trace lines from GPGPU-Sim runs."""

from __future__ import annotations

import argparse
import csv
import re
from pathlib import Path

BEGIN_RE = re.compile(
    r"paperrepro_kernel_begin\s+launch_index=(?P<launch_index>\S+)\s+"
    r"uid=(?P<uid>\S+)\s+name=(?P<name>\S+)\s+"
    r"grid=\((?P<grid>[^)]*)\)\s+block=\((?P<block>[^)]*)\)\s+"
    r"stream=(?P<stream>\S*)\s+cycle=(?P<cycle>\S*)"
)
END_RE = re.compile(
    r"paperrepro_kernel_end\s+launch_index=(?P<launch_index>\S+)\s+"
    r"uid=(?P<uid>\S+)\s+name=(?P<name>\S+)\s+"
    r"stream=(?P<stream>\S*)\s+cycle=(?P<cycle>\S*)"
)
LEGACY_RE = re.compile(
    r"kernel\s+(?P<uid>\d+):\s+'(?P<name>[^']+)'\s+transfer to GPU hardware scheduler"
)

FIELDS = [
    "run_id",
    "config_id",
    "paper_id",
    "paper_name",
    "app",
    "wrapper_path",
    "event",
    "trace_source",
    "launch_index",
    "kernel_uid",
    "kernel_name",
    "grid_x",
    "grid_y",
    "grid_z",
    "block_x",
    "block_y",
    "block_z",
    "stream_id",
    "begin_cycle",
    "end_cycle",
    "log_path",
    "notes",
]

APP_BY_PREFIX = {
    "bp": "backprop",
    "histo": "histo",
    "kmeans": "kmeans",
    "srad": "srad",
    "bfs": "bfs",
    "spmv": "spmv",
    "mri": "mri_q",
    "pathfinder": "pathfinder",
    "sgemm": "sgemm",
    "stencil": "stencil",
}


def read_csv(path: Path) -> list[dict[str, str]]:
    with path.open(newline="") as f:
        return list(csv.DictReader(f))


def write_csv(path: Path, rows: list[dict[str, str]]) -> None:
    path.parent.mkdir(parents=True, exist_ok=True)
    with path.open("w", newline="") as f:
        writer = csv.DictWriter(f, fieldnames=FIELDS, lineterminator="\n")
        writer.writeheader()
        writer.writerows(rows)


def split_xyz(value: str) -> tuple[str, str, str]:
    parts = [p.strip() for p in value.split(",")]
    while len(parts) < 3:
        parts.append("")
    return parts[0], parts[1], parts[2]


def app_from_paper_id(paper_id: str) -> str:
    prefix = paper_id.split("_")[0]
    return APP_BY_PREFIX.get(prefix, prefix)


def candidate_logs(run_dir: Path) -> list[Path]:
    names = ["combined.log", "stdout.log", "stderr.log"]
    logs = [run_dir / name for name in names if (run_dir / name).exists()]
    logs.extend(sorted(p for p in run_dir.glob("*.log") if p.name not in names))
    return logs


def manifest_rows(input_path: Path) -> list[dict[str, str]]:
    if input_path.is_file():
        return [{"run_id": input_path.stem, "run_dir": str(input_path.parent), "log_path": str(input_path)}]
    manifest = input_path / "run_manifest.csv"
    if manifest.exists():
        return read_csv(manifest)
    rows = []
    for log in sorted(input_path.rglob("*.log")):
        rows.append({"run_id": log.stem, "run_dir": str(log.parent), "log_path": str(log)})
    return rows


def parse_log(log_path: Path, row: dict[str, str]) -> list[dict[str, str]]:
    text = log_path.read_text(errors="replace")
    out: list[dict[str, str]] = []
    defaults = {
        "run_id": row.get("run_id", ""),
        "config_id": row.get("config_id", ""),
        "paper_id": row.get("paper_id", ""),
        "paper_name": row.get("paper_name", ""),
        "app": row.get("app", "") or app_from_paper_id(row.get("paper_id", "")),
        "wrapper_path": row.get("wrapper_path", ""),
        "log_path": str(log_path),
        "notes": "",
    }
    for match in BEGIN_RE.finditer(text):
        gx, gy, gz = split_xyz(match.group("grid"))
        bx, by, bz = split_xyz(match.group("block"))
        record = {field: "" for field in FIELDS}
        record.update(defaults)
        record.update(
            {
                "event": "begin",
                "trace_source": "paperrepro_kernel_begin",
                "launch_index": match.group("launch_index"),
                "kernel_uid": match.group("uid"),
                "kernel_name": match.group("name"),
                "grid_x": gx,
                "grid_y": gy,
                "grid_z": gz,
                "block_x": bx,
                "block_y": by,
                "block_z": bz,
                "stream_id": match.group("stream"),
                "begin_cycle": match.group("cycle"),
            }
        )
        out.append(record)
    for match in END_RE.finditer(text):
        record = {field: "" for field in FIELDS}
        record.update(defaults)
        record.update(
            {
                "event": "end",
                "trace_source": "paperrepro_kernel_end",
                "launch_index": match.group("launch_index"),
                "kernel_uid": match.group("uid"),
                "kernel_name": match.group("name"),
                "stream_id": match.group("stream"),
                "end_cycle": match.group("cycle"),
            }
        )
        out.append(record)
    if not out:
        legacy_index = 0
        for match in LEGACY_RE.finditer(text):
            legacy_index += 1
            record = {field: "" for field in FIELDS}
            record.update(defaults)
            record.update(
                {
                    "event": "begin",
                    "trace_source": "legacy_debug_kernel_transfer",
                    "launch_index": str(legacy_index),
                    "kernel_uid": match.group("uid"),
                    "kernel_name": match.group("name").replace(" ", "_"),
                    "notes": "legacy debug line has no grid/block/cycle fields",
                }
            )
            out.append(record)
    return out


def collect(input_path: Path) -> list[dict[str, str]]:
    rows: list[dict[str, str]] = []
    for row in manifest_rows(input_path):
        if row.get("log_path"):
            logs = [Path(row["log_path"])]
        else:
            logs = candidate_logs(Path(row.get("run_dir", "")))
        for log in logs:
            if log.exists():
                rows.extend(parse_log(log, row))
    rows.sort(key=lambda r: (r["run_id"], r["event"] != "begin", int(r["launch_index"] or "0")))
    return rows


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("input", type=Path, help="Run outdir, run_manifest parent, or log file")
    parser.add_argument("--output", type=Path, default=None)
    args = parser.parse_args()

    rows = collect(args.input)
    output = args.output or args.input / "kernel_trace.csv"
    write_csv(output, rows)
    print(f"kernel_trace_rows={len(rows)}")
    print(f"kernel_trace_csv={output}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
