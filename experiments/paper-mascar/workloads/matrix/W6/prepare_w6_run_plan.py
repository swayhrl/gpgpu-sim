#!/usr/bin/env python3
"""Prepare W6 activation-aware ready workload run plan."""

from __future__ import annotations

import csv
from pathlib import Path


REPO_ROOT = Path(__file__).resolve().parents[5]
WORKLOAD_DIR = REPO_ROOT / "experiments" / "paper-mascar" / "workloads"
W6_DIR = WORKLOAD_DIR / "matrix" / "W6"

TABLE_MANIFEST = WORKLOAD_DIR / "mascar_table_iii_workload_manifest.csv"
COMMAND_MANIFEST = WORKLOAD_DIR / "mascar_table_iii_command_manifest.csv"
W5_COUNTERS = WORKLOAD_DIR / "matrix" / "W5A" / "activation_counters.csv"
W5_MATRIX = WORKLOAD_DIR / "matrix" / "W5B" / "activation_matrix.csv"
W5C_SUBSET = WORKLOAD_DIR / "matrix" / "W5C" / "activation_ready_subset_manifest.csv"
W5C_COMMANDS = WORKLOAD_DIR / "matrix" / "W5C" / "w5c_iter2_command_manifest.csv"
CONFIG_MATRIX = W6_DIR / "w6_config_matrix.csv"


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
    "w5_m2_active",
    "w5_m3_active",
    "w5_m4_active",
    "w5c_m2_triggered",
    "w5c_m3_triggered",
    "w5c_m4_triggered",
    "selected_for_w6",
    "selection_reason",
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
    "expected_activation",
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
        writer.writerows(rows)


def active_flag(row: dict[str, str], *keys: str) -> str:
    for key in keys:
        if row.get(key, "0") == "1":
            return "1"
    return "0"


def main() -> int:
    table_rows = read_csv(TABLE_MANIFEST)
    command_rows = read_csv(COMMAND_MANIFEST)
    w5_rows = read_csv(W5_MATRIX) or read_csv(W5_COUNTERS)
    w5c_rows = read_csv(W5C_SUBSET)
    w5c_commands = read_csv(W5C_COMMANDS)
    config_rows = [r for r in read_csv(CONFIG_MATRIX) if r.get("enabled") == "1"]

    if len(table_rows) != 30:
        raise SystemExit(f"expected 30 Table III rows, found {len(table_rows)}")
    if not config_rows:
        raise SystemExit("no enabled W6 configs")

    command_by_id = {r["paper_id"]: r for r in command_rows}
    w5_by_id = {r["paper_id"]: r for r in w5_rows}
    w5c_by_id = {r["paper_id"]: r for r in w5c_rows}
    w5c_cmd_by_id = {r["paper_id"]: r for r in w5c_commands}

    workload_rows: list[dict[str, str]] = []
    ready_rows: list[dict[str, str]] = []
    activation_ready_rows: list[dict[str, str]] = []

    for table in table_rows:
        pid = table["paper_id"]
        command = command_by_id.get(pid, {})
        w5 = w5_by_id.get(pid, {})
        w5c = w5c_by_id.get(pid, {})
        w5c_cmd = w5c_cmd_by_id.get(pid, {})
        w5c_active = (
            w5c.get("M2_active_counter_triggered", "0") == "1"
            or w5c.get("M3_active_counter_triggered", "0") == "1"
            or w5c.get("M4_active_counter_triggered", "0") == "1"
        )
        ready = command.get("wrapper_status") == "ready"
        selected = ready
        if w5c_active:
            reason = "w5c_strict_activation_ready"
        elif ready and w5c_cmd:
            reason = "ready_candidate_w5c_input_no_strict_activation"
        elif ready:
            reason = "ready_wrapper_no_activation_input"
        elif command.get("wrapper_status") == "placeholder_phase_unknown":
            reason = "skip_phase_unknown"
        elif command.get("wrapper_status") == "placeholder_missing_binary":
            reason = "skip_missing_binary"
        elif command.get("wrapper_status") == "placeholder_missing_source":
            reason = "skip_missing_source"
        else:
            reason = "skip_unavailable"
        wrapper_path = command.get("wrapper_path", "")
        timeout = "1200"
        notes = command.get("notes", "")
        if ready and w5c_cmd:
            wrapper_path = w5c_cmd.get("wrapper_path", wrapper_path)
            timeout = w5c_cmd.get("timeout_sec", "1200") or "1200"
            notes = f"W5C candidate input; strict activation flags were zero. {w5c_cmd.get('notes', '')}"
        elif command.get("timeout_sec"):
            timeout = command["timeout_sec"]

        out = {
            "paper_id": pid,
            "paper_name": table["paper_name"],
            "paper_type": table["paper_type"],
            "paper_suite": table["paper_suite"],
            "paper_inst_per_l1_miss": table["paper_inst_per_l1_miss"],
            "wrapper_path": wrapper_path,
            "wrapper_status": command.get("wrapper_status", "unknown"),
            "availability_status": command.get("availability_status", table.get("availability_status", "unknown")),
            "phase_mapping_status": table.get("phase_mapping_status", "unknown"),
            "w5_m2_active": active_flag(w5, "m2_mp_owner_active", "M2_active_counter_triggered"),
            "w5_m3_active": active_flag(w5, "m3_hitonly_active", "M3_active_counter_triggered"),
            "w5_m4_active": active_flag(w5, "m4_reexec_active", "M4_active_counter_triggered"),
            "w5c_m2_triggered": w5c.get("M2_active_counter_triggered", "0"),
            "w5c_m3_triggered": w5c.get("M3_active_counter_triggered", "0"),
            "w5c_m4_triggered": w5c.get("M4_active_counter_triggered", "0"),
            "selected_for_w6": "1" if selected else "0",
            "selection_reason": reason,
            "timeout_sec": timeout,
            "notes": notes,
        }
        workload_rows.append(out)
        if ready:
            ready_rows.append(out)
        if w5c_active:
            activation_ready_rows.append(out)

    run_plan: list[dict[str, str]] = []
    command_ready_rows: list[dict[str, str]] = []
    command_activation_ready_rows: list[dict[str, str]] = []
    for workload in ready_rows:
        pid = workload["paper_id"]
        command = dict(command_by_id.get(pid, {}))
        w5c_cmd = w5c_cmd_by_id.get(pid)
        if w5c_cmd:
            command.update({k: v for k, v in w5c_cmd.items() if k in COMMAND_FIELDS})
            command["notes"] = f"W6 uses W5C candidate input; no strict activation found. {w5c_cmd.get('notes', '')}"
        else:
            command["notes"] = f"W6 uses W2 ready command. {command.get('notes', '')}"
        command_ready_rows.append({field: command.get(field, "") for field in COMMAND_FIELDS})
        if workload["w5c_m2_triggered"] == "1" or workload["w5c_m3_triggered"] == "1" or workload["w5c_m4_triggered"] == "1":
            command_activation_ready_rows.append({field: command.get(field, "") for field in COMMAND_FIELDS})
        for config in config_rows:
            priority = "1" if workload["paper_type"] == "M" else "2"
            expected = "none_seen_current_inputs"
            if workload["w5c_m2_triggered"] == "1" or workload["w5c_m3_triggered"] == "1" or workload["w5c_m4_triggered"] == "1":
                expected = "w5c_activation"
            run_plan.append(
                {
                    "run_group": "ready_candidates",
                    "config_id": config["config_id"],
                    "config_path": config["config_path"],
                    "paper_id": workload["paper_id"],
                    "paper_name": workload["paper_name"],
                    "paper_type": workload["paper_type"],
                    "wrapper_path": workload["wrapper_path"],
                    "wrapper_status": workload["wrapper_status"],
                    "selected_for_run": "1",
                    "run_priority": priority,
                    "timeout_sec": workload["timeout_sec"],
                    "expected_activation": expected,
                    "notes": workload["notes"],
                }
            )

    write_csv(W6_DIR / "w6_workload_manifest_all_tableiii.csv", workload_rows, WORKLOAD_FIELDS)
    write_csv(W6_DIR / "w6_workload_manifest_ready.csv", ready_rows, WORKLOAD_FIELDS)
    write_csv(W6_DIR / "w6_workload_manifest_activation_ready.csv", activation_ready_rows, WORKLOAD_FIELDS)
    write_csv(W6_DIR / "w6_command_manifest_ready.csv", command_ready_rows, COMMAND_FIELDS)
    write_csv(W6_DIR / "w6_command_manifest_activation_ready.csv", command_activation_ready_rows, COMMAND_FIELDS)
    write_csv(W6_DIR / "w6_run_plan.csv", run_plan, RUN_PLAN_FIELDS)

    print(f"all_tableiii_rows={len(workload_rows)}")
    print(f"ready_rows={len(ready_rows)}")
    print(f"activation_ready_rows={len(activation_ready_rows)}")
    print(f"run_plan_rows={len(run_plan)}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
