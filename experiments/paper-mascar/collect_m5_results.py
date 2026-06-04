#!/usr/bin/env python3
import csv
import re
import sys
from pathlib import Path

SCRIPT_DIR = Path(__file__).resolve().parent
OUT_CSV = SCRIPT_DIR / "m5_results.csv"
OUT_MD = SCRIPT_DIR / "m5_results_summary.md"

FIELDS = [
    "gpu_tot_sim_cycle",
    "gpu_tot_ipc",
    "paper_mascar_l1_sat_sample",
    "paper_mascar_l1_sat_sample_saturated",
    "paper_mascar_m2_ep_cycles",
    "paper_mascar_m2_mp_cycles",
    "paper_mascar_m2_owner_acquire",
    "paper_mascar_m2_nonowner_mem_block",
    "paper_mascar_m3_hitonly_access_attempt",
    "paper_mascar_m3_hitonly_access_hit",
    "paper_mascar_m3_hitonly_access_nack",
    "paper_mascar_m4_would_enqueue_attempt",
    "paper_mascar_m4_would_enqueue_success",
    "paper_mascar_m4_enqueue_success",
    "paper_mascar_m4_retry_attempt",
    "paper_mascar_m4_retry_hit",
    "paper_mascar_m4_retry_nack",
    "paper_mascar_m4_retry_requeue",
    "paper_mascar_m4_queue_occupancy_max",
    "cacheinst_L1D_miss_rate",
    "cacheinst_L1D_reservation_fail",
]

ALIASES = {
    "paper_mascar_m4_retry_requeue": "paper_mascar_m4_retry_requeue_tail",
    "paper_mascar_m4_queue_occupancy_max": "paper_mascar_m4_active_queue_max_occupancy",
}

VALUE_RE = re.compile(r"^\s*([A-Za-z0-9_]+)\s*=\s*([^\s]+)")


def parse_log(path: Path) -> dict:
    values = {}
    if not path.exists():
        return values
    with path.open(errors="replace") as fh:
        for line in fh:
            match = VALUE_RE.search(line)
            if not match:
                continue
            key, value = match.group(1), match.group(2)
            values[key] = value
    return values


def load_manifest(run_dir: Path) -> list:
    manifest = run_dir / "run_manifest.csv"
    if not manifest.exists():
        return []
    with manifest.open(newline="") as fh:
        return list(csv.DictReader(fh))


def write_empty(reason: str) -> None:
    headers = ["config_id", "workload_id", "exit_code", "result_status"] + FIELDS + ["log_path", "run_dir"]
    with OUT_CSV.open("w", newline="") as fh:
        writer = csv.DictWriter(fh, fieldnames=headers)
        writer.writeheader()
    OUT_MD.write_text(
        "# M5 Results Summary\n\n"
        "No runtime benchmark results collected.\n\n"
        f"Reason: {reason}\n",
        encoding="utf-8",
    )


def main() -> int:
    if len(sys.argv) > 1:
        run_dir = Path(sys.argv[1]).resolve()
    else:
        candidates = sorted((SCRIPT_DIR / "m5_runs").glob("*"))
        run_dir = candidates[-1].resolve() if candidates else None

    if run_dir is None or not run_dir.exists():
        write_empty("no M5 run directory was provided or found")
        return 0

    rows = load_manifest(run_dir)
    if not rows:
        write_empty(f"run manifest missing or empty in {run_dir}")
        return 0

    headers = ["config_id", "workload_id", "exit_code", "result_status"] + FIELDS + ["log_path", "run_dir"]
    out_rows = []
    for row in rows:
        log_path = Path(row.get("log_path", ""))
        values = parse_log(log_path)
        out = {
            "config_id": row.get("config_id", ""),
            "workload_id": row.get("workload_id", ""),
            "exit_code": row.get("exit_code", ""),
            "result_status": "completed" if row.get("exit_code", "") == "0" else "failed_or_timeout",
            "log_path": str(log_path),
            "run_dir": row.get("run_dir", ""),
        }
        for field in FIELDS:
            key = ALIASES.get(field, field)
            out[field] = values.get(field, values.get(key, ""))
        out_rows.append(out)

    with OUT_CSV.open("w", newline="") as fh:
        writer = csv.DictWriter(fh, fieldnames=headers)
        writer.writeheader()
        writer.writerows(out_rows)

    completed = sum(1 for row in out_rows if row["exit_code"] == "0")
    failed = len(out_rows) - completed
    lines = [
        "# M5 Results Summary",
        "",
        f"Run directory: `{run_dir}`",
        f"Rows: {len(out_rows)}",
        f"Completed: {completed}",
        f"Failed or timed out: {failed}",
        "",
        "| config | workload | exit | cycles | ipc | l1 samples | m2 mp | m3 attempts | m4 enqueue | m4 retry |",
        "|---|---|---:|---:|---:|---:|---:|---:|---:|---:|",
    ]
    for row in out_rows:
        lines.append(
            "| {config_id} | {workload_id} | {exit_code} | {cycles} | {ipc} | {l1} | {m2mp} | {m3} | {m4e} | {m4r} |".format(
                config_id=row["config_id"],
                workload_id=row["workload_id"],
                exit_code=row["exit_code"],
                cycles=row.get("gpu_tot_sim_cycle", ""),
                ipc=row.get("gpu_tot_ipc", ""),
                l1=row.get("paper_mascar_l1_sat_sample", ""),
                m2mp=row.get("paper_mascar_m2_mp_cycles", ""),
                m3=row.get("paper_mascar_m3_hitonly_access_attempt", ""),
                m4e=row.get("paper_mascar_m4_enqueue_success", ""),
                m4r=row.get("paper_mascar_m4_retry_attempt", ""),
            )
        )
    lines.extend([
        "",
        "Interpretation notes:",
        "",
        "- These are focused runtime sanity results, not paper-comparable speedup data.",
        "- Zero M3 hit-only attempts can mean the short workload did not create MP non-owner load opportunities.",
        "- M4 enqueue/retry nonzero values indicate the active load re-execution queue ran on this workload.",
    ])
    OUT_MD.write_text("\n".join(lines) + "\n", encoding="utf-8")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
