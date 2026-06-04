#!/usr/bin/env python3
"""Analyze W8 focused activation sweep results."""

from __future__ import annotations

import argparse
import csv
import math
from collections import Counter, defaultdict
from pathlib import Path


REPO_ROOT = Path(__file__).resolve().parents[5]
W8_DIR = REPO_ROOT / "experiments" / "paper-mascar" / "workloads" / "matrix" / "W8"
RESULTS_DIR = REPO_ROOT / "experiments" / "paper-mascar" / "workloads" / "results" / "W8"

CONFIG_ORDER = ["baseline_off", "m2_owner_sched", "m3_hitonly_nack", "m4_reexec_load"]


def read_csv(path: Path) -> list[dict[str, str]]:
    if not path.exists():
        return []
    with path.open(newline="") as f:
        return list(csv.DictReader(f))


def write_csv(path: Path, rows: list[dict[str, object]], fields: list[str]) -> None:
    path.parent.mkdir(parents=True, exist_ok=True)
    with path.open("w", newline="") as f:
        writer = csv.DictWriter(f, fieldnames=fields, lineterminator="\n")
        writer.writeheader()
        writer.writerows([{field: row.get(field, "") for field in fields} for row in rows])


def to_int(value: str) -> int:
    if value is None or value == "":
        return 0
    try:
        return int(float(value))
    except ValueError:
        return 0


def to_float(value: str) -> float | None:
    if value is None or value == "":
        return None
    try:
        return float(value)
    except ValueError:
        return None


def yes(value: bool) -> str:
    return "1" if value else "0"


def geomean(values: list[float]) -> float | None:
    valid = [v for v in values if v > 0]
    if not valid:
        return None
    return math.exp(sum(math.log(v) for v in valid) / len(valid))


def fmt(value: float | None) -> str:
    return "" if value is None else f"{value:.6f}"


def m2_active(row: dict[str, str]) -> bool:
    return (
        to_int(row.get("paper_mascar_m2_mp_cycles", "0")) > 0
        or to_int(row.get("paper_mascar_m2_owner_acquire", "0")) > 0
        or to_int(row.get("paper_mascar_m2_nonowner_mem_block", "0")) > 0
    )


def m3_active(row: dict[str, str]) -> bool:
    return to_int(row.get("paper_mascar_m3_hitonly_access_attempt", "0")) > 0


def m4_active(row: dict[str, str]) -> bool:
    return (
        to_int(row.get("paper_mascar_m4_enqueue_success", "0")) > 0
        or to_int(row.get("paper_mascar_m4_retry_attempt", "0")) > 0
        or to_int(row.get("paper_mascar_m4_retry_requeue", "0")) > 0
    )


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument(
        "results_csv",
        type=Path,
        nargs="?",
        default=RESULTS_DIR / "w8_latest_results.csv",
    )
    args = parser.parse_args()

    result_rows = read_csv(args.results_csv)
    run_plan_rows = read_csv(W8_DIR / "w8_run_plan.csv")
    if not result_rows:
        raise SystemExit(f"missing or empty results: {args.results_csv}")
    expected_pairs = {(r["paper_id"], r["config_id"]) for r in run_plan_rows}
    observed_pairs = {(r["paper_id"], r["config_id"]) for r in result_rows}
    missing_pairs = sorted(expected_pairs - observed_pairs)

    baseline_by_workload = {
        row["paper_id"]: row for row in result_rows if row.get("config_id") == "baseline_off"
    }
    trend_rows: list[dict[str, object]] = []
    activation_by_workload: dict[str, dict[str, bool]] = defaultdict(
        lambda: {"m2": False, "m3": False, "m4": False}
    )
    ratio_values: dict[str, list[float]] = defaultdict(list)

    for row in result_rows:
        pid = row["paper_id"]
        cfg = row["config_id"]
        base = baseline_by_workload.get(pid, {})
        cycles = to_float(row.get("gpu_tot_sim_cycle", ""))
        base_cycles = to_float(base.get("gpu_tot_sim_cycle", ""))
        speedup = base_cycles / cycles if base_cycles and cycles and cycles > 0 else None
        active_m2 = m2_active(row)
        active_m3 = m3_active(row)
        active_m4 = m4_active(row)
        if cfg != "baseline_off":
            activation_by_workload[pid]["m2"] |= active_m2
            activation_by_workload[pid]["m3"] |= active_m3
            activation_by_workload[pid]["m4"] |= active_m4
            if speedup:
                ratio_values[cfg].append(speedup)
        trend_rows.append(
            {
                "paper_id": pid,
                "paper_name": row.get("paper_name", ""),
                "paper_type": row.get("paper_type", ""),
                "config_id": cfg,
                "classification": row.get("classification", ""),
                "explicit_pass": row.get("explicit_pass", "0"),
                "gpu_tot_sim_cycle": row.get("gpu_tot_sim_cycle", ""),
                "gpu_tot_ipc": row.get("gpu_tot_ipc", ""),
                "baseline_cycles": base.get("gpu_tot_sim_cycle", ""),
                "cycle_speedup_vs_baseline": fmt(speedup),
                "l1_sat_samples": row.get("paper_mascar_l1_sat_sample", "0"),
                "l1_sat_saturated_samples": row.get("paper_mascar_l1_sat_sample_saturated", "0"),
                "m2_mp_cycles": row.get("paper_mascar_m2_mp_cycles", "0"),
                "m2_owner_acquire": row.get("paper_mascar_m2_owner_acquire", "0"),
                "m2_nonowner_mem_block": row.get("paper_mascar_m2_nonowner_mem_block", "0"),
                "m3_hitonly_attempt": row.get("paper_mascar_m3_hitonly_access_attempt", "0"),
                "m3_hitonly_nack": row.get("paper_mascar_m3_hitonly_access_nack", "0"),
                "m4_enqueue_success": row.get("paper_mascar_m4_enqueue_success", "0"),
                "m4_retry_attempt": row.get("paper_mascar_m4_retry_attempt", "0"),
                "m4_retry_nack": row.get("paper_mascar_m4_retry_nack", "0"),
                "m2_active": yes(active_m2),
                "m3_active": yes(active_m3),
                "m4_active": yes(active_m4),
                "any_active": yes(active_m2 or active_m3 or active_m4),
            }
        )

    activation_rows: list[dict[str, object]] = []
    for pid in sorted({r["paper_id"] for r in result_rows}):
        rows = [r for r in result_rows if r["paper_id"] == pid]
        name = rows[0].get("paper_name", "")
        ptype = rows[0].get("paper_type", "")
        active = activation_by_workload[pid]
        activation_rows.append(
            {
                "paper_id": pid,
                "paper_name": name,
                "paper_type": ptype,
                "runs": len(rows),
                "completed_runs": sum(1 for r in rows if r.get("classification", "").startswith("completed")),
                "explicit_pass_runs": sum(1 for r in rows if r.get("explicit_pass", "0") == "1"),
                "m2_active": yes(active["m2"]),
                "m3_active": yes(active["m3"]),
                "m4_active": yes(active["m4"]),
                "any_active": yes(active["m2"] or active["m3"] or active["m4"]),
                "activation_status": "active_counter_observed"
                if (active["m2"] or active["m3"] or active["m4"])
                else "no_active_counter_observed",
            }
        )

    geomean_rows = []
    for cfg in CONFIG_ORDER:
        if cfg == "baseline_off":
            continue
        geomean_rows.append(
            {
                "config_id": cfg,
                "valid_pairs": len(ratio_values[cfg]),
                "cycle_speedup_geomean": fmt(geomean(ratio_values[cfg])),
            }
        )

    write_csv(
        W8_DIR / "w8_trend_results.csv",
        trend_rows,
        [
            "paper_id",
            "paper_name",
            "paper_type",
            "config_id",
            "classification",
            "explicit_pass",
            "gpu_tot_sim_cycle",
            "gpu_tot_ipc",
            "baseline_cycles",
            "cycle_speedup_vs_baseline",
            "l1_sat_samples",
            "l1_sat_saturated_samples",
            "m2_mp_cycles",
            "m2_owner_acquire",
            "m2_nonowner_mem_block",
            "m3_hitonly_attempt",
            "m3_hitonly_nack",
            "m4_enqueue_success",
            "m4_retry_attempt",
            "m4_retry_nack",
            "m2_active",
            "m3_active",
            "m4_active",
            "any_active",
        ],
    )
    write_csv(
        W8_DIR / "w8_activation_summary.csv",
        activation_rows,
        [
            "paper_id",
            "paper_name",
            "paper_type",
            "runs",
            "completed_runs",
            "explicit_pass_runs",
            "m2_active",
            "m3_active",
            "m4_active",
            "any_active",
            "activation_status",
        ],
    )
    write_csv(
        W8_DIR / "w8_geomean_summary.csv",
        geomean_rows,
        ["config_id", "valid_pairs", "cycle_speedup_geomean"],
    )

    classifications = Counter(r.get("classification", "") for r in result_rows)
    m2_count = sum(r["m2_active"] == "1" for r in activation_rows)
    m3_count = sum(r["m3_active"] == "1" for r in activation_rows)
    m4_count = sum(r["m4_active"] == "1" for r in activation_rows)
    any_count = sum(r["any_active"] == "1" for r in activation_rows)

    with (W8_DIR / "w8_trend_summary.md").open("w") as f:
        f.write("# Mascar W8 Trend Summary\n\n")
        f.write("W8 is a focused activation sweep over the W7 subset: `spmv`, `mri_q`, and `pathfinder`.\n\n")
        f.write("## Run Counts\n\n")
        f.write(f"- run plan rows: {len(run_plan_rows)}\n")
        f.write(f"- result rows: {len(result_rows)}\n")
        f.write(f"- missing planned pairs: {len(missing_pairs)}\n\n")
        f.write("## Status Counts\n\n")
        for key, value in sorted(classifications.items()):
            f.write(f"- {key}: {value}\n")
        f.write("\n## Active Workload Counts\n\n")
        f.write(f"- M2 active workloads: {m2_count}\n")
        f.write(f"- M3 active workloads: {m3_count}\n")
        f.write(f"- M4 active workloads: {m4_count}\n")
        f.write(f"- Any active workloads: {any_count}\n\n")
        f.write("## Preliminary Cycle Geomeans\n\n")
        f.write("These are focused smoke/full-sweep ratios against `baseline_off`, not paper-comparable speedup claims.\n\n")
        f.write("| config | valid pairs | cycle speedup geomean |\n")
        f.write("| --- | ---: | ---: |\n")
        for row in geomean_rows:
            f.write(f"| {row['config_id']} | {row['valid_pairs']} | {row['cycle_speedup_geomean']} |\n")
        f.write("\n## Caveats\n\n")
        f.write("- W8 does not run the full Table III workload set.\n")
        f.write("- `completed_stats_found` rows are not explicit correctness passes.\n")
        f.write("- M3 hit-only activation remains separate from M2/M4 activation and may remain zero.\n")

    print(f"trend_rows={len(trend_rows)}")
    print(f"activation_rows={len(activation_rows)}")
    print(f"m2_active_workloads={m2_count}")
    print(f"m3_active_workloads={m3_count}")
    print(f"m4_active_workloads={m4_count}")
    print(f"any_active_workloads={any_count}")
    print(f"missing_pairs={len(missing_pairs)}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
