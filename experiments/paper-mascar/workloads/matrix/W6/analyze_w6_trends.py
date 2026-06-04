#!/usr/bin/env python3
"""Analyze W6 ready workload sweep results.

The W6 sweep is activation-aware: performance ratios are only interpreted as
mechanism trends when M2/M3/M4 active counters are nonzero. Current W5C/W6 inputs
are expected to run, but not necessarily trigger active Mascar behavior.
"""

from __future__ import annotations

import csv
import math
from collections import Counter, defaultdict
from pathlib import Path


REPO_ROOT = Path(__file__).resolve().parents[5]
W6_DIR = REPO_ROOT / "experiments" / "paper-mascar" / "workloads" / "matrix" / "W6"
RESULTS_DIR = REPO_ROOT / "experiments" / "paper-mascar" / "workloads" / "results" / "W6"

RESULTS_CSV = RESULTS_DIR / "w6_latest_results.csv"
ALL_TABLEIII_CSV = W6_DIR / "w6_workload_manifest_all_tableiii.csv"
READY_CSV = W6_DIR / "w6_workload_manifest_ready.csv"
RUN_PLAN_CSV = W6_DIR / "w6_run_plan.csv"

TREND_RESULTS_CSV = RESULTS_DIR / "w6_trend_results.csv"
ACTIVATION_SUMMARY_CSV = RESULTS_DIR / "w6_activation_summary.csv"
GEOMEAN_SUMMARY_CSV = RESULTS_DIR / "w6_geomean_summary.csv"
COVERAGE_MANIFEST_CSV = RESULTS_DIR / "w6_coverage_manifest.csv"
TREND_SUMMARY_MD = RESULTS_DIR / "w6_trend_summary.md"

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
        for row in rows:
            writer.writerow({field: row.get(field, "") for field in fields})


def to_float(value: str) -> float | None:
    if value is None or value == "":
        return None
    try:
        return float(value)
    except ValueError:
        return None


def to_int(value: str) -> int:
    if value is None or value == "":
        return 0
    try:
        return int(float(value))
    except ValueError:
        return 0


def bool_int(value: bool) -> str:
    return "1" if value else "0"


def geomean(values: list[float]) -> float | None:
    valid = [v for v in values if v > 0]
    if not valid:
        return None
    return math.exp(sum(math.log(v) for v in valid) / len(valid))


def fmt_float(value: float | None, digits: int = 6) -> str:
    if value is None:
        return ""
    return f"{value:.{digits}f}"


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
    result_rows = read_csv(RESULTS_CSV)
    table_rows = read_csv(ALL_TABLEIII_CSV)
    ready_rows = read_csv(READY_CSV)
    run_plan_rows = read_csv(RUN_PLAN_CSV)
    if not result_rows:
        raise SystemExit(f"missing or empty results: {RESULTS_CSV}")
    if len(table_rows) != 30:
        raise SystemExit(f"expected 30 Table III rows, found {len(table_rows)}")

    result_by_pair = {(r["paper_id"], r["config_id"]): r for r in result_rows}
    baseline_by_workload = {
        r["paper_id"]: r
        for r in result_rows
        if r.get("config_id") == "baseline_off"
    }

    trend_rows: list[dict[str, object]] = []
    activation_by_workload: dict[str, dict[str, bool]] = defaultdict(
        lambda: {"m2": False, "m3": False, "m4": False}
    )

    for row in result_rows:
        pid = row["paper_id"]
        config_id = row["config_id"]
        baseline = baseline_by_workload.get(pid, {})
        cycles = to_float(row.get("gpu_tot_sim_cycle", ""))
        ipc = to_float(row.get("gpu_tot_ipc", ""))
        baseline_cycles = to_float(baseline.get("gpu_tot_sim_cycle", ""))
        baseline_ipc = to_float(baseline.get("gpu_tot_ipc", ""))
        speedup = None
        ipc_ratio = None
        if cycles and baseline_cycles and cycles > 0:
            speedup = baseline_cycles / cycles
        if ipc and baseline_ipc and baseline_ipc > 0:
            ipc_ratio = ipc / baseline_ipc

        active_m2 = m2_active(row)
        active_m3 = m3_active(row)
        active_m4 = m4_active(row)
        if config_id != "baseline_off":
            activation_by_workload[pid]["m2"] |= active_m2
            activation_by_workload[pid]["m3"] |= active_m3
            activation_by_workload[pid]["m4"] |= active_m4
        any_active = active_m2 or active_m3 or active_m4
        explicit_pass = row.get("explicit_pass", "0") == "1"
        classification = row.get("classification", "")
        completed = classification.startswith("completed")

        trend_rows.append(
            {
                "paper_id": pid,
                "paper_name": row.get("paper_name", ""),
                "paper_type": row.get("paper_type", ""),
                "config_id": config_id,
                "classification": classification,
                "completed": bool_int(completed),
                "explicit_pass": bool_int(explicit_pass),
                "correctness_basis": "explicit_pass" if explicit_pass else "stats_only_no_explicit_pass",
                "gpu_tot_sim_cycle": row.get("gpu_tot_sim_cycle", ""),
                "gpu_tot_ipc": row.get("gpu_tot_ipc", ""),
                "baseline_cycles": baseline.get("gpu_tot_sim_cycle", ""),
                "baseline_ipc": baseline.get("gpu_tot_ipc", ""),
                "cycle_speedup_vs_baseline": fmt_float(speedup),
                "ipc_ratio_vs_baseline": fmt_float(ipc_ratio),
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
                "m2_active": bool_int(active_m2),
                "m3_active": bool_int(active_m3),
                "m4_active": bool_int(active_m4),
                "any_m2_m3_m4_active": bool_int(any_active),
                "trend_interpretation": "mechanism_not_activated" if not any_active else "mechanism_counter_active",
            }
        )

    activation_rows: list[dict[str, object]] = []
    for pid in sorted({r["paper_id"] for r in result_rows}):
        rows = [r for r in result_rows if r["paper_id"] == pid]
        name = rows[0].get("paper_name", "")
        ptype = rows[0].get("paper_type", "")
        active = activation_by_workload[pid]
        l1_samples = sum(to_int(r.get("paper_mascar_l1_sat_sample", "0")) for r in rows)
        l1_sat = sum(to_int(r.get("paper_mascar_l1_sat_sample_saturated", "0")) for r in rows)
        activation_rows.append(
            {
                "paper_id": pid,
                "paper_name": name,
                "paper_type": ptype,
                "runs": len(rows),
                "completed_runs": sum(1 for r in rows if r.get("classification", "").startswith("completed")),
                "explicit_pass_runs": sum(1 for r in rows if r.get("explicit_pass", "0") == "1"),
                "l1_sat_samples": l1_samples,
                "l1_sat_saturated_samples": l1_sat,
                "m2_active": bool_int(active["m2"]),
                "m3_active": bool_int(active["m3"]),
                "m4_active": bool_int(active["m4"]),
                "any_m2_m3_m4_active": bool_int(active["m2"] or active["m3"] or active["m4"]),
                "activation_status": "no_activation_under_current_inputs"
                if not (active["m2"] or active["m3"] or active["m4"])
                else "active_counter_observed",
            }
        )

    ratio_by_config_type: dict[tuple[str, str], list[float]] = defaultdict(list)
    pass_by_config_type: dict[tuple[str, str], list[bool]] = defaultdict(list)
    active_by_config_type: Counter[tuple[str, str]] = Counter()
    for row in trend_rows:
        config_id = str(row["config_id"])
        if config_id == "baseline_off":
            continue
        key = (config_id, str(row["paper_type"]))
        speedup = to_float(str(row["cycle_speedup_vs_baseline"]))
        if speedup and str(row["classification"]).startswith("completed"):
            ratio_by_config_type[key].append(speedup)
            pass_by_config_type[key].append(row["explicit_pass"] == "1")
        if row["any_m2_m3_m4_active"] == "1":
            active_by_config_type[key] += 1

    geomean_rows: list[dict[str, object]] = []
    for config_id in CONFIG_ORDER:
        if config_id == "baseline_off":
            continue
        all_values: list[float] = []
        all_passes: list[bool] = []
        all_active = 0
        for ptype in ["M", "C"]:
            values = ratio_by_config_type.get((config_id, ptype), [])
            passes = pass_by_config_type.get((config_id, ptype), [])
            all_values.extend(values)
            all_passes.extend(passes)
            all_active += active_by_config_type.get((config_id, ptype), 0)
            geomean_rows.append(
                {
                    "config_id": config_id,
                    "paper_type": ptype,
                    "valid_pairs": len(values),
                    "explicit_pass_pairs": sum(1 for p in passes if p),
                    "active_workloads": active_by_config_type.get((config_id, ptype), 0),
                    "cycle_speedup_geomean": fmt_float(geomean(values)),
                    "interpretation": "no_activation_no_mechanism_trend"
                    if active_by_config_type.get((config_id, ptype), 0) == 0
                    else "active_counter_observed",
                }
            )
        geomean_rows.append(
            {
                "config_id": config_id,
                "paper_type": "all_ready",
                "valid_pairs": len(all_values),
                "explicit_pass_pairs": sum(1 for p in all_passes if p),
                "active_workloads": all_active,
                "cycle_speedup_geomean": fmt_float(geomean(all_values)),
                "interpretation": "no_activation_no_mechanism_trend"
                if all_active == 0
                else "active_counter_observed",
            }
        )

    latest_status_by_workload: dict[str, dict[str, str]] = defaultdict(dict)
    for row in result_rows:
        latest_status_by_workload[row["paper_id"]][row["config_id"]] = row.get("classification", "")
    coverage_rows: list[dict[str, object]] = []
    ready_ids = {r["paper_id"] for r in ready_rows}
    planned_ids = {r["paper_id"] for r in run_plan_rows}
    activation_ids = {
        r["paper_id"]
        for r in activation_rows
        if r["any_m2_m3_m4_active"] == "1"
    }
    for table in table_rows:
        pid = table["paper_id"]
        status_by_config = latest_status_by_workload.get(pid, {})
        coverage_rows.append(
            {
                "paper_id": pid,
                "paper_name": table.get("paper_name", ""),
                "paper_type": table.get("paper_type", ""),
                "paper_suite": table.get("paper_suite", ""),
                "wrapper_status": table.get("wrapper_status", ""),
                "availability_status": table.get("availability_status", ""),
                "selected_for_w6": bool_int(pid in planned_ids),
                "ready_candidate": bool_int(pid in ready_ids),
                "activation_ready_observed": bool_int(pid in activation_ids),
                "baseline_off": status_by_config.get("baseline_off", "not_run"),
                "m2_owner_sched": status_by_config.get("m2_owner_sched", "not_run"),
                "m3_hitonly_nack": status_by_config.get("m3_hitonly_nack", "not_run"),
                "m4_reexec_load": status_by_config.get("m4_reexec_load", "not_run"),
                "coverage_status": "ready_swept_no_activation"
                if pid in ready_ids and pid not in activation_ids
                else ("ready_swept_active" if pid in activation_ids else "not_ready_not_run"),
            }
        )

    write_csv(
        TREND_RESULTS_CSV,
        trend_rows,
        [
            "paper_id",
            "paper_name",
            "paper_type",
            "config_id",
            "classification",
            "completed",
            "explicit_pass",
            "correctness_basis",
            "gpu_tot_sim_cycle",
            "gpu_tot_ipc",
            "baseline_cycles",
            "baseline_ipc",
            "cycle_speedup_vs_baseline",
            "ipc_ratio_vs_baseline",
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
            "any_m2_m3_m4_active",
            "trend_interpretation",
        ],
    )
    write_csv(
        ACTIVATION_SUMMARY_CSV,
        activation_rows,
        [
            "paper_id",
            "paper_name",
            "paper_type",
            "runs",
            "completed_runs",
            "explicit_pass_runs",
            "l1_sat_samples",
            "l1_sat_saturated_samples",
            "m2_active",
            "m3_active",
            "m4_active",
            "any_m2_m3_m4_active",
            "activation_status",
        ],
    )
    write_csv(
        GEOMEAN_SUMMARY_CSV,
        geomean_rows,
        [
            "config_id",
            "paper_type",
            "valid_pairs",
            "explicit_pass_pairs",
            "active_workloads",
            "cycle_speedup_geomean",
            "interpretation",
        ],
    )
    write_csv(
        COVERAGE_MANIFEST_CSV,
        coverage_rows,
        [
            "paper_id",
            "paper_name",
            "paper_type",
            "paper_suite",
            "wrapper_status",
            "availability_status",
            "selected_for_w6",
            "ready_candidate",
            "activation_ready_observed",
            "baseline_off",
            "m2_owner_sched",
            "m3_hitonly_nack",
            "m4_reexec_load",
            "coverage_status",
        ],
    )

    classifications = Counter(r.get("classification", "") for r in result_rows)
    active_counts = Counter()
    for row in activation_rows:
        if row["m2_active"] == "1":
            active_counts["m2"] += 1
        if row["m3_active"] == "1":
            active_counts["m3"] += 1
        if row["m4_active"] == "1":
            active_counts["m4"] += 1
        if row["any_m2_m3_m4_active"] == "1":
            active_counts["any"] += 1

    with TREND_SUMMARY_MD.open("w") as f:
        f.write("# W6 Trend Summary\n\n")
        f.write("W6 found no activation-ready workload under the current W5C inputs. ")
        f.write("No M2/M3/M4 active counters were triggered in the ready workload sweep, ")
        f.write("so the cycle ratios below are smoke-level comparability checks, not Mascar mechanism benefit claims.\n\n")
        f.write("## Run Coverage\n\n")
        f.write(f"- Table III rows: {len(table_rows)}\n")
        f.write(f"- Ready candidate workloads swept: {len(ready_rows)}\n")
        f.write(f"- Configs per ready workload: {len(CONFIG_ORDER)}\n")
        f.write(f"- Result rows: {len(result_rows)}\n")
        f.write(f"- Planned rows: {len(run_plan_rows)}\n\n")
        f.write("## Status Counts\n\n")
        for key, value in sorted(classifications.items()):
            f.write(f"- {key}: {value}\n")
        f.write("\n## Mechanism Activation\n\n")
        f.write(f"- M2 active workload count: {active_counts['m2']}\n")
        f.write(f"- M3 active workload count: {active_counts['m3']}\n")
        f.write(f"- M4 active workload count: {active_counts['m4']}\n")
        f.write(f"- Any M2/M3/M4 active workload count: {active_counts['any']}\n")
        f.write("- Conclusion: no activation-ready workload found yet; no M2-M4 activation under current inputs.\n\n")
        f.write("## Cycle Ratio Geomeans\n\n")
        f.write("These ratios are recorded for debugging only because active mechanism counters are zero.\n\n")
        f.write("| config | type | valid pairs | explicit pass pairs | active workloads | cycle speedup geomean | interpretation |\n")
        f.write("| --- | --- | ---: | ---: | ---: | ---: | --- |\n")
        for row in geomean_rows:
            f.write(
                f"| {row['config_id']} | {row['paper_type']} | {row['valid_pairs']} | "
                f"{row['explicit_pass_pairs']} | {row['active_workloads']} | "
                f"{row['cycle_speedup_geomean']} | {row['interpretation']} |\n"
            )
        f.write("\n## Correctness Caveat\n\n")
        f.write(
            "Only rows with `completed_explicit_pass` have an explicit pass signal. "
            "`completed_stats_found` rows are simulator-completed smoke rows with stats, "
            "not correctness-pass evidence.\n"
        )

    print(f"trend_rows={len(trend_rows)}")
    print(f"activation_rows={len(activation_rows)}")
    print(f"coverage_rows={len(coverage_rows)}")
    print(f"m2_active_workloads={active_counts['m2']}")
    print(f"m3_active_workloads={active_counts['m3']}")
    print(f"m4_active_workloads={active_counts['m4']}")
    print(f"any_active_workloads={active_counts['any']}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
