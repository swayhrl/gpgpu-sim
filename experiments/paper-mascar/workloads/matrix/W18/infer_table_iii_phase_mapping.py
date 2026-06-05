#!/usr/bin/env python3
"""Infer proposed Table III phase mapping from W18 kernel launch traces."""

from __future__ import annotations

import argparse
import csv
import re
from collections import defaultdict
from pathlib import Path

FIELDS = [
    "paper_id",
    "paper_name",
    "paper_type",
    "app",
    "phase_mapping_status_before_w18",
    "phase_mapping_status_after_w18",
    "mapping_confidence",
    "local_launch_index",
    "local_kernel_uid",
    "local_kernel_name",
    "grid",
    "block",
    "evidence",
    "notes",
]


def read_csv(path: Path) -> list[dict[str, str]]:
    with path.open(newline="") as f:
        return list(csv.DictReader(f))


def write_csv(path: Path, rows: list[dict[str, str]], fields: list[str]) -> None:
    path.parent.mkdir(parents=True, exist_ok=True)
    with path.open("w", newline="") as f:
        writer = csv.DictWriter(f, fieldnames=fields, lineterminator="\n")
        writer.writeheader()
        writer.writerows(rows)


def app_from_pid(pid: str) -> str:
    if pid.startswith("bp_"):
        return "backprop"
    if pid.startswith("histo_"):
        return "histo"
    if pid.startswith("kmeans_"):
        return "kmeans"
    if pid.startswith("srad_"):
        return "srad"
    return pid.split("_")[0]


def suffix_index(pid: str) -> int | None:
    m = re.search(r"_(\d+)$", pid)
    return int(m.group(1)) if m else None


def grid(row: dict[str, str]) -> str:
    return f"({row.get('grid_x','')},{row.get('grid_y','')},{row.get('grid_z','')})"


def block(row: dict[str, str]) -> str:
    return f"({row.get('block_x','')},{row.get('block_y','')},{row.get('block_z','')})"


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--command-manifest", type=Path, default=Path("experiments/paper-mascar/workloads/mascar_table_iii_command_manifest.csv"))
    parser.add_argument("--targets", type=Path, default=Path("experiments/paper-mascar/workloads/matrix/W18/w18a_phase_pending_targets.csv"))
    parser.add_argument("--trace", type=Path, default=Path("experiments/paper-mascar/workloads/results/W18/w18_latest_kernel_trace.csv"))
    parser.add_argument("--out-dir", type=Path, default=Path("experiments/paper-mascar/workloads/matrix/W18"))
    args = parser.parse_args()

    canonical = read_csv(args.command_manifest)
    targets = {row["paper_id"]: row for row in read_csv(args.targets)} if args.targets.exists() else {}
    trace_rows = read_csv(args.trace) if args.trace.exists() else []
    begins_by_pid: dict[str, list[dict[str, str]]] = defaultdict(list)
    begins_by_app: dict[str, list[dict[str, str]]] = defaultdict(list)
    for row in trace_rows:
        if row.get("event") != "begin":
            continue
        if row.get("trace_source") != "paperrepro_kernel_begin":
            continue
        begins_by_pid[row.get("paper_id", "")].append(row)
        begins_by_app[row.get("app", "")].append(row)
    for rows in begins_by_pid.values():
        rows.sort(key=lambda r: int(r.get("launch_index") or "0"))
    for rows in begins_by_app.values():
        rows.sort(key=lambda r: int(r.get("launch_index") or "0"))

    proposed = []
    unresolved = []
    for row in canonical:
        pid = row["paper_id"]
        app = app_from_pid(pid)
        before = row.get("phase_mapping_status", "")
        rec = {field: "" for field in FIELDS}
        rec.update(
            {
                "paper_id": pid,
                "paper_name": row.get("paper_name", pid),
                "paper_type": row.get("paper_type", ""),
                "app": app,
                "phase_mapping_status_before_w18": before,
            }
        )
        if pid in targets:
            pid_begins = begins_by_pid.get(pid, [])
            app_begins = begins_by_app.get(app, [])
            idx = suffix_index(pid)
            selected = None
            if idx is not None and idx <= len(pid_begins):
                selected = pid_begins[idx - 1]
            elif idx is not None and idx <= len(app_begins):
                selected = app_begins[idx - 1]
            elif len(pid_begins) == 1:
                selected = pid_begins[0]
            if selected and idx is not None and len(pid_begins) >= idx:
                rec.update(
                    {
                        "phase_mapping_status_after_w18": "mapped_by_launch_order",
                        "mapping_confidence": "inferred_order",
                        "local_launch_index": selected.get("launch_index", ""),
                        "local_kernel_uid": selected.get("kernel_uid", ""),
                        "local_kernel_name": selected.get("kernel_name", ""),
                        "grid": grid(selected),
                        "block": block(selected),
                        "evidence": f"paper_id trace has {len(pid_begins)} begin lines; suffix index {idx} selected",
                        "notes": "Proposed mapping only; requires GPT review before canonical manifest update.",
                    }
                )
            elif selected:
                rec.update(
                    {
                        "phase_mapping_status_after_w18": "app_level_trace_available",
                        "mapping_confidence": "app_level_trace_available",
                        "local_launch_index": selected.get("launch_index", ""),
                        "local_kernel_uid": selected.get("kernel_uid", ""),
                        "local_kernel_name": selected.get("kernel_name", ""),
                        "grid": grid(selected),
                        "block": block(selected),
                        "evidence": f"trace exists but does not prove suffix-to-launch mapping; app begin lines={len(app_begins)}",
                        "notes": "Do not claim exact phase mapping from this evidence alone.",
                    }
                )
            else:
                rec.update(
                    {
                        "phase_mapping_status_after_w18": "unresolved_no_kernel_trace",
                        "mapping_confidence": "unresolved",
                        "evidence": "no paperrepro_kernel_begin line collected for this phase-pending row",
                        "notes": "Needs rerun or deeper instrumentation.",
                    }
                )
                unresolved.append(rec.copy())
        else:
            status = before or row.get("wrapper_status", "")
            rec.update(
                {
                    "phase_mapping_status_after_w18": status,
                    "mapping_confidence": "not_w18_target",
                    "evidence": "not in W17 app_level_pending_kernel_trace target set",
                    "notes": row.get("notes", ""),
                }
            )
        proposed.append(rec)

    write_csv(args.out_dir / "table_iii_phase_mapping.csv", proposed, FIELDS)
    write_csv(args.out_dir / "table_iii_phase_mapping_proposed_manifest.csv", proposed, FIELDS)
    write_csv(args.out_dir / "table_iii_phase_mapping_unresolved.csv", unresolved, FIELDS)
    print(f"proposed_rows={len(proposed)}")
    print(f"unresolved_rows={len(unresolved)}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
