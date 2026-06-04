#!/usr/bin/env python3
"""Prepare W8 focused sweep run plan from W7 activation-ready subset."""

from __future__ import annotations

import csv
from pathlib import Path


REPO_ROOT = Path(__file__).resolve().parents[5]
WORKLOAD_DIR = REPO_ROOT / "experiments" / "paper-mascar" / "workloads"
W7_DIR = WORKLOAD_DIR / "matrix" / "W7"
W8_DIR = WORKLOAD_DIR / "matrix" / "W8"

TABLE_MANIFEST = WORKLOAD_DIR / "mascar_table_iii_workload_manifest.csv"
W7_COMMAND_MANIFEST = W7_DIR / "w7_iter1_command_manifest.csv"
W7_SUBSET = W7_DIR / "activation_ready_subset_manifest.csv"
W7_COUNTERS = W7_DIR / "w7_counter_summary.csv"
CONFIG_MATRIX = W8_DIR / "w8_config_matrix.csv"


WORKLOAD_FIELDS = [
    "paper_id",
    "paper_name",
    "paper_type",
    "paper_suite",
    "paper_inst_per_l1_miss",
    "wrapper_path",
    "wrapper_status",
    "availability_status",
    "phase_mapping_status",
    "selected_for_w8",
    "selection_tier",
    "expected_m2_active",
    "expected_m3_active",
    "expected_m4_active",
    "timeout_sec",
    "notes",
]

RUN_PLAN_FIELDS = [
    "run_group",
    "config_id",
    "config_path",
    "paper_id",
    "paper_name",
    "paper_type",
    "wrapper_path",
    "wrapper_status",
    "selected_for_run",
    "run_priority",
    "timeout_sec",
    "expected_m2_active",
    "expected_m3_active",
    "expected_m4_active",
    "notes",
]

COMMAND_FIELDS = [
    "paper_id",
    "paper_name",
    "paper_type",
    "availability_status",
    "wrapper_path",
    "wrapper_status",
    "build_required",
    "build_command",
    "run_working_dir",
    "run_command",
    "input_size",
    "timeout_sec",
    "dry_run_status",
    "notes",
]


def read_csv(path: Path) -> list[dict[str, str]]:
    if not path.exists():
        return []
    with path.open(newline="") as f:
        return list(csv.DictReader(f))


def write_csv(path: Path, rows: list[dict[str, str]], fields: list[str]) -> None:
    path.parent.mkdir(parents=True, exist_ok=True)
    with path.open("w", newline="") as f:
        writer = csv.DictWriter(f, fieldnames=fields, lineterminator="\n")
        writer.writeheader()
        writer.writerows([{field: row.get(field, "") for field in fields} for row in rows])


def main() -> int:
    table_rows = read_csv(TABLE_MANIFEST)
    w7_commands = read_csv(W7_COMMAND_MANIFEST)
    subset_rows = read_csv(W7_SUBSET)
    counter_rows = read_csv(W7_COUNTERS)
    config_rows = [r for r in read_csv(CONFIG_MATRIX) if r.get("enabled") == "1"]
    if len(table_rows) != 30:
        raise SystemExit(f"expected 30 Table III rows, found {len(table_rows)}")
    if not subset_rows:
        raise SystemExit(f"missing W7 activation-ready subset: {W7_SUBSET}")
    if not config_rows:
        raise SystemExit("no enabled W8 configs")

    command_by_id = {row["paper_id"]: row for row in w7_commands}
    subset_by_id = {row["paper_id"]: row for row in subset_rows}
    counter_by_id = {row["paper_id"]: row for row in counter_rows}

    workload_rows: list[dict[str, str]] = []
    command_subset_rows: list[dict[str, str]] = []
    run_plan_rows: list[dict[str, str]] = []

    for table in table_rows:
        pid = table["paper_id"]
        command = command_by_id.get(pid, {})
        subset = subset_by_id.get(pid, {})
        counters = counter_by_id.get(pid, {})
        selected = pid in subset_by_id
        if selected:
            selection_tier = "w7_activation_ready"
            wrapper_path = subset.get("wrapper_path", command.get("wrapper_path", ""))
            wrapper_status = command.get("wrapper_status", "ready")
            availability = command.get("availability_status", "available")
            timeout = command.get("timeout_sec", "600") or "600"
            notes = f"W8 selected from W7 activation-ready subset. {subset.get('notes', '')}"
        elif command.get("wrapper_status") == "ready":
            selection_tier = "ready_not_active_w7"
            wrapper_path = command.get("wrapper_path", "")
            wrapper_status = "ready"
            availability = command.get("availability_status", "available")
            timeout = command.get("timeout_sec", "600") or "600"
            notes = f"Ready but not in W7 activation-ready subset. {counters.get('notes', '')}"
        else:
            selection_tier = "placeholder_or_unavailable"
            wrapper_path = command.get("wrapper_path", "")
            wrapper_status = command.get("wrapper_status", table.get("run_command_status", "unknown"))
            availability = table.get("availability_status", "unknown")
            timeout = "600"
            notes = table.get("notes", "")

        out = {
            "paper_id": pid,
            "paper_name": table["paper_name"],
            "paper_type": table["paper_type"],
            "paper_suite": table["paper_suite"],
            "paper_inst_per_l1_miss": table["paper_inst_per_l1_miss"],
            "wrapper_path": wrapper_path,
            "wrapper_status": wrapper_status,
            "availability_status": availability,
            "phase_mapping_status": table.get("phase_mapping_status", "unknown"),
            "selected_for_w8": "1" if selected else "0",
            "selection_tier": selection_tier,
            "expected_m2_active": subset.get("M2_active_counter_triggered", "0"),
            "expected_m3_active": subset.get("M3_active_counter_triggered", "0"),
            "expected_m4_active": subset.get("M4_active_counter_triggered", "0"),
            "timeout_sec": timeout,
            "notes": notes,
        }
        workload_rows.append(out)

        if not selected:
            continue
        command_row = dict(command)
        command_row["notes"] = f"W8 focused sweep; {subset.get('notes', command.get('notes', ''))}"
        command_subset_rows.append({field: command_row.get(field, "") for field in COMMAND_FIELDS})
        for config in config_rows:
            run_plan_rows.append(
                {
                    "run_group": "w7_activation_ready",
                    "config_id": config["config_id"],
                    "config_path": config["config_path"],
                    "paper_id": out["paper_id"],
                    "paper_name": out["paper_name"],
                    "paper_type": out["paper_type"],
                    "wrapper_path": out["wrapper_path"],
                    "wrapper_status": out["wrapper_status"],
                    "selected_for_run": "1",
                    "run_priority": "1" if out["paper_type"] == "M" else "2",
                    "timeout_sec": timeout,
                    "expected_m2_active": out["expected_m2_active"],
                    "expected_m3_active": out["expected_m3_active"],
                    "expected_m4_active": out["expected_m4_active"],
                    "notes": out["notes"],
                }
            )

    write_csv(W8_DIR / "w8_workload_manifest.csv", workload_rows, WORKLOAD_FIELDS)
    write_csv(W8_DIR / "w8_command_manifest_subset.csv", command_subset_rows, COMMAND_FIELDS)
    write_csv(W8_DIR / "w8_run_plan.csv", run_plan_rows, RUN_PLAN_FIELDS)

    print(f"tableiii_rows={len(workload_rows)}")
    print(f"selected_workloads={len(command_subset_rows)}")
    print(f"enabled_configs={len(config_rows)}")
    print(f"run_plan_rows={len(run_plan_rows)}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
