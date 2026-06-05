#!/usr/bin/env python3
"""Analyze Mascar workload matrix results for W9-W14 stages."""

from __future__ import annotations

import argparse
import csv
import math
from collections import Counter, defaultdict
from pathlib import Path


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


def to_int(value: str | None) -> int:
    if value in (None, ""):
        return 0
    try:
        return int(float(value))
    except ValueError:
        return 0


def to_float(value: str | None) -> float | None:
    if value in (None, ""):
        return None
    try:
        return float(value)
    except ValueError:
        return None


def yes(value: bool) -> str:
    return "1" if value else "0"


def fmt(value: float | None) -> str:
    return "" if value is None else f"{value:.6f}"


def geomean(values: list[float]) -> float | None:
    valid = [v for v in values if v > 0]
    if not valid:
        return None
    return math.exp(sum(math.log(v) for v in valid) / len(valid))


def m2_active(row: dict[str, str]) -> bool:
    return (
        to_int(row.get("paper_mascar_m2_mp_cycles")) > 0
        or to_int(row.get("paper_mascar_m2_owner_acquire")) > 0
        or to_int(row.get("paper_mascar_m2_nonowner_mem_block")) > 0
    )


def m3_strict_active(row: dict[str, str]) -> bool:
    return to_int(row.get("paper_mascar_m3_hitonly_access_attempt")) > 0


def m3_probe_observed(row: dict[str, str]) -> bool:
    return (
        to_int(row.get("paper_mascar_m3_probe_attempt")) > 0
        or to_int(row.get("paper_mascar_m3_nonowner_lsu_probe_allowed")) > 0
        or to_int(row.get("paper_mascar_m3_hitonly_access_attempt")) > 0
    )


def m4_active(row: dict[str, str]) -> bool:
    return (
        to_int(row.get("paper_mascar_m4_enqueue_success")) > 0
        or to_int(row.get("paper_mascar_m4_retry_attempt")) > 0
        or to_int(row.get("paper_mascar_m4_retry_requeue")) > 0
    )


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--results", required=True, type=Path)
    parser.add_argument("--manifest", type=Path)
    parser.add_argument("--outdir", required=True, type=Path)
    parser.add_argument("--prefix", required=True)
    parser.add_argument("--title", required=True)
    args = parser.parse_args()

    rows = read_csv(args.results)
    if not rows:
        raise SystemExit(f"missing or empty results CSV: {args.results}")

    args.outdir.mkdir(parents=True, exist_ok=True)
    baselines = {r["paper_id"]: r for r in rows if r.get("config_id") == "baseline_off"}
    ratio_values: dict[str, list[float]] = defaultdict(list)
    trend_rows: list[dict[str, object]] = []
    activation: dict[str, dict[str, bool]] = defaultdict(
        lambda: {"m2": False, "m3": False, "m3_probe": False, "m4": False}
    )

    for row in rows:
        pid = row["paper_id"]
        cfg = row["config_id"]
        base = baselines.get(pid, {})
        cycles = to_float(row.get("gpu_tot_sim_cycle"))
        base_cycles = to_float(base.get("gpu_tot_sim_cycle"))
        speedup = base_cycles / cycles if base_cycles and cycles and cycles > 0 else None
        active_m2 = m2_active(row)
        active_m3 = m3_strict_active(row)
        observed_m3 = m3_probe_observed(row)
        active_m4 = m4_active(row)
        if cfg != "baseline_off":
            activation[pid]["m2"] |= active_m2
            activation[pid]["m3"] |= active_m3
            activation[pid]["m3_probe"] |= observed_m3
            activation[pid]["m4"] |= active_m4
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
                "m2_mp_cycles": row.get("paper_mascar_m2_mp_cycles", "0"),
                "m2_owner_acquire": row.get("paper_mascar_m2_owner_acquire", "0"),
                "m2_nonowner_mem_block": row.get("paper_mascar_m2_nonowner_mem_block", "0"),
                "m3_hitonly_attempt": row.get("paper_mascar_m3_hitonly_access_attempt", "0"),
                "m3_hitonly_hit": row.get("paper_mascar_m3_hitonly_access_hit", "0"),
                "m3_hitonly_nack": row.get("paper_mascar_m3_hitonly_access_nack", "0"),
                "m3_probe_attempt": row.get("paper_mascar_m3_probe_attempt", "0"),
                "m3_probe_hit": row.get("paper_mascar_m3_probe_hit", "0"),
                "m3_probe_nack": row.get("paper_mascar_m3_probe_nack", "0"),
                "m3_nonowner_lsu_probe_allowed": row.get("paper_mascar_m3_nonowner_lsu_probe_allowed", "0"),
                "m4_enqueue_success": row.get("paper_mascar_m4_enqueue_success", "0"),
                "m4_retry_attempt": row.get("paper_mascar_m4_retry_attempt", "0"),
                "m4_retry_requeue": row.get("paper_mascar_m4_retry_requeue", "0"),
                "m2_active": yes(active_m2),
                "m3_strict_active": yes(active_m3),
                "m3_probe_observed": yes(observed_m3),
                "m4_active": yes(active_m4),
                "any_active": yes(active_m2 or active_m3 or active_m4),
            }
        )

    manifest_rows = read_csv(args.manifest) if args.manifest else []
    ids = []
    seen = set()
    for source in [manifest_rows, rows]:
        for row in source:
            pid = row.get("paper_id", "")
            if pid and pid not in seen:
                seen.add(pid)
                ids.append(pid)

    by_pid = defaultdict(list)
    for row in rows:
        by_pid[row["paper_id"]].append(row)

    activation_rows = []
    for pid in ids:
        result_rows = by_pid.get(pid, [])
        ref = result_rows[0] if result_rows else next((r for r in manifest_rows if r.get("paper_id") == pid), {})
        flags = activation[pid]
        activation_rows.append(
            {
                "paper_id": pid,
                "paper_name": ref.get("paper_name", ""),
                "paper_type": ref.get("paper_type", ""),
                "wrapper_status": ref.get("wrapper_status", ""),
                "runs": len(result_rows),
                "completed_runs": sum(1 for r in result_rows if r.get("classification", "").startswith("completed")),
                "explicit_pass_runs": sum(1 for r in result_rows if r.get("explicit_pass") == "1"),
                "m2_active": yes(flags["m2"]),
                "m3_strict_active": yes(flags["m3"]),
                "m3_probe_observed": yes(flags["m3_probe"]),
                "m4_active": yes(flags["m4"]),
                "any_strict_active": yes(flags["m2"] or flags["m3"] or flags["m4"]),
            }
        )

    geomean_rows = []
    for cfg in sorted({r["config_id"] for r in rows if r.get("config_id") != "baseline_off"}):
        geomean_rows.append(
            {
                "config_id": cfg,
                "valid_pairs": len(ratio_values[cfg]),
                "cycle_speedup_geomean": fmt(geomean(ratio_values[cfg])),
            }
        )

    trend_fields = [
        "paper_id", "paper_name", "paper_type", "config_id", "classification",
        "explicit_pass", "gpu_tot_sim_cycle", "gpu_tot_ipc", "baseline_cycles",
        "cycle_speedup_vs_baseline", "m2_mp_cycles", "m2_owner_acquire",
        "m2_nonowner_mem_block", "m3_hitonly_attempt", "m3_hitonly_hit",
        "m3_hitonly_nack", "m3_probe_attempt", "m3_probe_hit", "m3_probe_nack",
        "m3_nonowner_lsu_probe_allowed", "m4_enqueue_success", "m4_retry_attempt",
        "m4_retry_requeue", "m2_active", "m3_strict_active",
        "m3_probe_observed", "m4_active", "any_active",
    ]
    activation_fields = [
        "paper_id", "paper_name", "paper_type", "wrapper_status", "runs",
        "completed_runs", "explicit_pass_runs", "m2_active", "m3_strict_active",
        "m3_probe_observed", "m4_active", "any_strict_active",
    ]
    geomean_fields = ["config_id", "valid_pairs", "cycle_speedup_geomean"]

    write_csv(args.outdir / f"{args.prefix}_trend_results.csv", trend_rows, trend_fields)
    write_csv(args.outdir / f"{args.prefix}_activation_summary.csv", activation_rows, activation_fields)
    write_csv(args.outdir / f"{args.prefix}_geomean_summary.csv", geomean_rows, geomean_fields)

    status = Counter(r.get("classification", "") for r in rows)
    m2_count = sum(1 for r in activation_rows if r["m2_active"] == "1")
    m3_count = sum(1 for r in activation_rows if r["m3_strict_active"] == "1")
    m3_probe_count = sum(1 for r in activation_rows if r["m3_probe_observed"] == "1")
    m4_count = sum(1 for r in activation_rows if r["m4_active"] == "1")
    active_list = [r["paper_id"] for r in activation_rows if r["any_strict_active"] == "1"]

    with (args.outdir / f"{args.prefix}_summary.md").open("w") as f:
        f.write(f"# {args.title}\n\n")
        f.write(f"- result rows: {len(rows)}\n")
        f.write(f"- manifest rows: {len(manifest_rows) if manifest_rows else 'not provided'}\n")
        f.write(f"- M2 active workloads: {m2_count}\n")
        f.write(f"- M3 strict active workloads: {m3_count}\n")
        f.write(f"- M3 probe-observed workloads: {m3_probe_count}\n")
        f.write(f"- M4 active workloads: {m4_count}\n")
        f.write(f"- strict active workload list: {', '.join(active_list) if active_list else 'none'}\n\n")
        f.write("## Classification Counts\n\n")
        for key, value in sorted(status.items()):
            f.write(f"- {key}: {value}\n")
        f.write("\n## Cycle Geomeans\n\n")
        for row in geomean_rows:
            f.write(f"- {row['config_id']}: valid_pairs={row['valid_pairs']}, geomean={row['cycle_speedup_geomean']}\n")

    print(f"rows={len(rows)}")
    print(f"m2_active_workloads={m2_count}")
    print(f"m3_strict_active_workloads={m3_count}")
    print(f"m3_probe_observed_workloads={m3_probe_count}")
    print(f"m4_active_workloads={m4_count}")
    print(f"active_workloads={','.join(active_list)}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
