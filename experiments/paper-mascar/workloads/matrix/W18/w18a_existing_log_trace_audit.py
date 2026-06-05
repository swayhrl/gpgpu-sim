#!/usr/bin/env python3
"""Audit W17 phase-pending rows and existing logs for W18 kernel trace planning."""

from __future__ import annotations

import csv
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
REPO = Path(__file__).resolve().parents[5]
MATRIX_W17 = ROOT / "matrix" / "W17"
AUDIT_W18 = ROOT / "audit" / "W18"
OUT = ROOT / "matrix" / "W18" / "w18a_phase_pending_targets.csv"

FIELDS = [
    "paper_id",
    "paper_name",
    "paper_type",
    "app",
    "wrapper_path",
    "phase_mapping_status_before_w18",
    "trace_config_id",
    "trace_needed",
    "mapping_goal",
    "notes",
]


def read_csv(path: Path) -> list[dict[str, str]]:
    with path.open(newline="") as f:
        return list(csv.DictReader(f))


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


def main() -> int:
    phase_path = MATRIX_W17 / "w17_phase_pending_manifest.csv"
    ready_path = MATRIX_W17 / "w17_ready_manifest.csv"
    if not phase_path.exists():
        raise SystemExit(f"missing {phase_path}")
    phase_rows = read_csv(phase_path)
    ready_by_id = {row["paper_id"]: row for row in read_csv(ready_path)} if ready_path.exists() else {}

    out_rows = []
    for phase in phase_rows:
        rid = phase["paper_id"]
        ready = ready_by_id.get(rid, {})
        out_rows.append(
            {
                "paper_id": rid,
                "paper_name": ready.get("paper_name", rid),
                "paper_type": ready.get("paper_type", ""),
                "app": phase.get("app", "") or app_from_pid(rid),
                "wrapper_path": phase.get("current_wrapper", ready.get("wrapper_path", "")),
                "phase_mapping_status_before_w18": phase.get("phase_mapping_status", "app_level_pending_kernel_trace"),
                "trace_config_id": "kernel_trace_baseline_off",
                "trace_needed": "yes",
                "mapping_goal": "map Table III row to local kernel launch index/name using structured begin/end trace",
                "notes": phase.get("notes", ""),
            }
        )

    OUT.parent.mkdir(parents=True, exist_ok=True)
    with OUT.open("w", newline="") as f:
        writer = csv.DictWriter(f, fieldnames=FIELDS, lineterminator="\n")
        writer.writeheader()
        writer.writerows(out_rows)

    source_grep = AUDIT_W18 / "w18a_kernel_source_grep.txt"
    log_grep = AUDIT_W18 / "w18a_existing_log_grep.txt"
    source_hits = source_grep.read_text(errors="replace").count("kernel") if source_grep.exists() else 0
    existing_hits = log_grep.read_text(errors="replace").count("paperrepro_kernel") if log_grep.exists() else 0
    print(f"phase_pending_targets={len(out_rows)}")
    print(f"source_grep_kernel_token_hits={source_hits}")
    print(f"existing_structured_trace_hits={existing_hits}")
    print(f"targets_csv={OUT}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
