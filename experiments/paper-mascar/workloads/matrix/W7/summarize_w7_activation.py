#!/usr/bin/env python3
"""Summarize W7 activation search counters."""

from __future__ import annotations

import argparse
import csv
from pathlib import Path


W7_DIR = Path(__file__).resolve().parent
DEFAULT_RESULTS = W7_DIR / "results" / "iter1_actual"
COUNTER_SUMMARY = W7_DIR / "w7_counter_summary.csv"
SUBSET_MANIFEST = W7_DIR / "activation_ready_subset_manifest.csv"
NOTES_MD = W7_DIR / "notes.md"

SUMMARY_FIELDS = [
    "paper_id",
    "paper_type",
    "active_config",
    "classification",
    "gpu_tot_sim_cycle",
    "paper_mascar_l1_sat_sample",
    "paper_mascar_l1_sat_sample_saturated",
    "paper_mascar_m2_mp_cycles",
    "paper_mascar_m2_owner_acquire",
    "paper_mascar_m2_nonowner_mem_block",
    "paper_mascar_m3_hitonly_access_attempt",
    "paper_mascar_m3_hitonly_access_nack",
    "paper_mascar_m4_enqueue_success",
    "paper_mascar_m4_retry_attempt",
    "paper_mascar_m4_retry_nack",
    "M2_active_counter_triggered",
    "M3_active_counter_triggered",
    "M4_active_counter_triggered",
    "input_modified",
    "notes",
]

SUBSET_FIELDS = [
    "paper_id",
    "wrapper_path",
    "active_config",
    "M2_active_counter_triggered",
    "M3_active_counter_triggered",
    "M4_active_counter_triggered",
    "input_modified",
    "notes",
]


def read_csv(path: Path) -> list[dict[str, str]]:
    if not path.exists():
        return []
    with path.open(newline="") as f:
        return list(csv.DictReader(f))


def write_csv(path: Path, rows: list[dict[str, str]], fields: list[str]) -> None:
    with path.open("w", newline="") as f:
        writer = csv.DictWriter(f, fieldnames=fields, lineterminator="\n")
        writer.writeheader()
        writer.writerows([{field: row.get(field, "") for field in fields} for row in rows])


def as_int(value: str) -> int:
    if value is None or value == "":
        return 0
    try:
        return int(float(value))
    except ValueError:
        return 0


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("results_csv", type=Path, nargs="?", default=DEFAULT_RESULTS / "results.csv")
    parser.add_argument("--manifest", type=Path, default=W7_DIR / "w7_iter1_command_manifest.csv")
    args = parser.parse_args()

    results = read_csv(args.results_csv)
    manifest_rows = read_csv(args.manifest)
    manifest_by_id = {row["paper_id"]: row for row in manifest_rows}
    if not results:
        raise SystemExit(f"missing or empty results: {args.results_csv}")

    summary_rows: list[dict[str, str]] = []
    subset_rows: list[dict[str, str]] = []
    for row in results:
        pid = row["paper_id"]
        manifest = manifest_by_id.get(pid, {})
        m2 = (
            as_int(row.get("paper_mascar_m2_mp_cycles", "0")) > 0
            or as_int(row.get("paper_mascar_m2_owner_acquire", "0")) > 0
            or as_int(row.get("paper_mascar_m2_nonowner_mem_block", "0")) > 0
        )
        m3 = as_int(row.get("paper_mascar_m3_hitonly_access_attempt", "0")) > 0
        m4 = (
            as_int(row.get("paper_mascar_m4_enqueue_success", "0")) > 0
            or as_int(row.get("paper_mascar_m4_retry_attempt", "0")) > 0
            or as_int(row.get("paper_mascar_m4_retry_nack", "0")) > 0
        )
        active = m2 or m3 or m4
        notes = (
            f"{manifest.get('input_size', '')}; "
            f"classification={row.get('classification', '')}; "
            f"l1_sat_samples={row.get('paper_mascar_l1_sat_sample', '0')}; "
            f"l1_sat_saturated={row.get('paper_mascar_l1_sat_sample_saturated', '0')}; "
        )
        notes += "activation observed" if active else "no active M2/M3/M4 counter under W7 iter1 input"
        out = {
            "paper_id": pid,
            "paper_type": row.get("paper_type", ""),
            "active_config": row.get("config_id", ""),
            "classification": row.get("classification", ""),
            "gpu_tot_sim_cycle": row.get("gpu_tot_sim_cycle", ""),
            "paper_mascar_l1_sat_sample": row.get("paper_mascar_l1_sat_sample", "0"),
            "paper_mascar_l1_sat_sample_saturated": row.get("paper_mascar_l1_sat_sample_saturated", "0"),
            "paper_mascar_m2_mp_cycles": row.get("paper_mascar_m2_mp_cycles", "0"),
            "paper_mascar_m2_owner_acquire": row.get("paper_mascar_m2_owner_acquire", "0"),
            "paper_mascar_m2_nonowner_mem_block": row.get("paper_mascar_m2_nonowner_mem_block", "0"),
            "paper_mascar_m3_hitonly_access_attempt": row.get("paper_mascar_m3_hitonly_access_attempt", "0"),
            "paper_mascar_m3_hitonly_access_nack": row.get("paper_mascar_m3_hitonly_access_nack", "0"),
            "paper_mascar_m4_enqueue_success": row.get("paper_mascar_m4_enqueue_success", "0"),
            "paper_mascar_m4_retry_attempt": row.get("paper_mascar_m4_retry_attempt", "0"),
            "paper_mascar_m4_retry_nack": row.get("paper_mascar_m4_retry_nack", "0"),
            "M2_active_counter_triggered": "1" if m2 else "0",
            "M3_active_counter_triggered": "1" if m3 else "0",
            "M4_active_counter_triggered": "1" if m4 else "0",
            "input_modified": "yes",
            "notes": notes,
        }
        summary_rows.append(out)
        if active:
            subset_rows.append(
                {
                    "paper_id": pid,
                    "wrapper_path": manifest.get("wrapper_path", ""),
                    "active_config": row.get("config_id", ""),
                    "M2_active_counter_triggered": "1" if m2 else "0",
                    "M3_active_counter_triggered": "1" if m3 else "0",
                    "M4_active_counter_triggered": "1" if m4 else "0",
                    "input_modified": "yes",
                    "notes": notes,
                }
            )

    write_csv(COUNTER_SUMMARY, summary_rows, SUMMARY_FIELDS)
    write_csv(SUBSET_MANIFEST, subset_rows, SUBSET_FIELDS)

    m2_count = sum(row["M2_active_counter_triggered"] == "1" for row in summary_rows)
    m3_count = sum(row["M3_active_counter_triggered"] == "1" for row in summary_rows)
    m4_count = sum(row["M4_active_counter_triggered"] == "1" for row in summary_rows)
    any_count = sum(
        row["M2_active_counter_triggered"] == "1"
        or row["M3_active_counter_triggered"] == "1"
        or row["M4_active_counter_triggered"] == "1"
        for row in summary_rows
    )

    with NOTES_MD.open("a") as f:
        f.write("\n## Iteration 1 Result\n\n")
        f.write(f"results_csv: `{args.results_csv}`\n\n")
        f.write(f"- rows: {len(summary_rows)}\n")
        f.write(f"- M2 active workloads: {m2_count}\n")
        f.write(f"- M3 active workloads: {m3_count}\n")
        f.write(f"- M4 active workloads: {m4_count}\n")
        f.write(f"- any active workloads: {any_count}\n")
        if any_count == 0:
            f.write("- conclusion: W7 iter1 larger inputs still did not trigger active M2/M3/M4 counters.\n")
        else:
            f.write("- conclusion: W7 iter1 found at least one activation-ready workload candidate.\n")

    print(f"rows={len(summary_rows)}")
    print(f"m2_active_workloads={m2_count}")
    print(f"m3_active_workloads={m3_count}")
    print(f"m4_active_workloads={m4_count}")
    print(f"any_active_workloads={any_count}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
