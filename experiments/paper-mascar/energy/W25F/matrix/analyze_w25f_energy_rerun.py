#!/usr/bin/env python3
from __future__ import annotations

import csv
import math
import statistics
import subprocess
import time
from pathlib import Path

ROOT = Path(__file__).resolve().parents[5]
W25F = ROOT / "experiments/paper-mascar/energy/W25F"
MATRIX = W25F / "matrix"
RESULTS = W25F / "results"
AUDIT = W25F / "audit"
DOC = ROOT / "docs/papers/mascar_w25f_energy_rerun_trend_report.md"
BASE = "energy_baseline_off"
M4 = "energy_m4_reexec_load"
FREQ_HZ = 1132.0e6
FREQ_SOURCE = "configs/hrl-repro/SM7_QV100_mascar_energy_baseline_off/gpgpusim.config -gpgpu_clock_domains 1132.0:1132.0:1132.0:850.0"


def read_csv(path: Path) -> list[dict[str, str]]:
    if not path.exists():
        return []
    with path.open(newline="") as f:
        return list(csv.DictReader(f))


def write_csv(path: Path, rows: list[dict[str, str]], fields: list[str]) -> None:
    path.parent.mkdir(parents=True, exist_ok=True)
    with path.open("w", newline="") as f:
        w = csv.DictWriter(f, fieldnames=fields, lineterminator="\n")
        w.writeheader(); w.writerows(rows)


def fnum(value: str) -> float | None:
    try:
        if value == "" or value is None:
            return None
        return float(value)
    except ValueError:
        return None


def ratio(a: float | None, b: float | None) -> float | None:
    if a is None or b is None or b == 0:
        return None
    return a / b


def fmt(v: float | None) -> str:
    return "" if v is None or not math.isfinite(v) else f"{v:.6g}"


def geomean(values: list[float]) -> float | None:
    vals = [v for v in values if v > 0 and math.isfinite(v)]
    if not vals:
        return None
    return math.exp(sum(math.log(v) for v in vals) / len(vals))


def main() -> int:
    start_ts = int(Path("/tmp/mascar_w25f_start_ts").read_text().strip()) if Path("/tmp/mascar_w25f_start_ts").exists() else int(time.time())
    rows = read_csv(RESULTS / "w25f_energy_actual_results.csv")
    if not rows:
        raise SystemExit("missing W25F actual results")
    artifacts = read_csv(RESULTS / "w25f_power_artifact_check.csv")
    artifact_by_run = {r["run_id"]: r for r in artifacts}
    workload_rows = read_csv(MATRIX / "w25f_energy_workload_manifest.csv")
    type_by_id = {r["workload_id"]: r.get("paper_type", "") for r in workload_rows}
    by_pair = {(r["paper_id"], r["config_id"]): r for r in rows}
    workloads = [r["workload_id"] for r in workload_rows]
    trend = []
    for wid in workloads:
        b = by_pair.get((wid, BASE), {})
        m = by_pair.get((wid, M4), {})
        bc = fnum(b.get("gpu_tot_sim_cycle", "")); mc = fnum(m.get("gpu_tot_sim_cycle", ""))
        bi = fnum(b.get("gpu_tot_ipc", "")); mi = fnum(m.get("gpu_tot_ipc", ""))
        bkp = fnum(b.get("kernel_avg_power", "")); mkp = fnum(m.get("kernel_avg_power", ""))
        bgp = fnum(b.get("gpu_tot_avg_power", "")); mgp = fnum(m.get("gpu_tot_avg_power", ""))
        be = bgp * (bc / FREQ_HZ) if bgp is not None and bc is not None else None
        me = mgp * (mc / FREQ_HZ) if mgp is not None and mc is not None else None
        er = ratio(me, be)
        trend.append({
            "workload_id": wid,
            "paper_type": type_by_id.get(wid, ""),
            "baseline_status": b.get("classification", "missing"),
            "m4_status": m.get("classification", "missing"),
            "baseline_cycles": b.get("gpu_tot_sim_cycle", ""),
            "m4_cycles": m.get("gpu_tot_sim_cycle", ""),
            "baseline_ipc": b.get("gpu_tot_ipc", ""),
            "m4_ipc": m.get("gpu_tot_ipc", ""),
            "cycle_ratio": fmt(ratio(mc, bc)),
            "ipc_ratio": fmt(ratio(mi, bi)),
            "baseline_kernel_avg_power": b.get("kernel_avg_power", ""),
            "m4_kernel_avg_power": m.get("kernel_avg_power", ""),
            "baseline_gpu_tot_avg_power": b.get("gpu_tot_avg_power", ""),
            "m4_gpu_tot_avg_power": m.get("gpu_tot_avg_power", ""),
            "kernel_power_ratio": fmt(ratio(mkp, bkp)),
            "gpu_total_power_ratio": fmt(ratio(mgp, bgp)),
            "baseline_derived_energy_j": fmt(be),
            "m4_derived_energy_j": fmt(me),
            "derived_energy_ratio": fmt(er),
            "derived_energy_saving": fmt(1 - er if er is not None else None),
            "artifact_status": f"baseline={artifact_by_run.get(b.get('run_id',''),{}).get('has_gpu_tot_avg_power','')};m4={artifact_by_run.get(m.get('run_id',''),{}).get('has_gpu_tot_avg_power','')}",
            "correctness_status": "explicit_pass" if b.get("explicit_pass") == "1" and m.get("explicit_pass") == "1" else "stats_only_not_correctness_pass",
            "notes": "current-simulator trend only; not paper GPUWattch/GTX480",
        })
    fields = ["workload_id","paper_type","baseline_status","m4_status","baseline_cycles","m4_cycles","baseline_ipc","m4_ipc","cycle_ratio","ipc_ratio","baseline_kernel_avg_power","m4_kernel_avg_power","baseline_gpu_tot_avg_power","m4_gpu_tot_avg_power","kernel_power_ratio","gpu_total_power_ratio","baseline_derived_energy_j","m4_derived_energy_j","derived_energy_ratio","derived_energy_saving","artifact_status","correctness_status","notes"]
    write_csv(RESULTS / "w25f_energy_trend_results.csv", trend, fields)
    availability = []
    for r in rows:
        availability.append({
            "run_id": r["run_id"], "workload_id": r["paper_id"], "config_id": r["config_id"], "classification": r["classification"], "has_stats": r["has_stats"],
            "energy_fields_found": r["energy_fields_found"], "has_kernel_avg_power": "1" if r.get("kernel_avg_power") else "0", "has_gpu_tot_avg_power": "1" if r.get("gpu_tot_avg_power") else "0",
            "has_power_artifacts": artifact_by_run.get(r["run_id"], {}).get("power_artifacts_dir_exists", "0"), "artifact_count": artifact_by_run.get(r["run_id"], {}).get("artifact_count", "0")
        })
    write_csv(RESULTS / "w25f_energy_availability_matrix.csv", availability, ["run_id","workload_id","config_id","classification","has_stats","energy_fields_found","has_kernel_avg_power","has_gpu_tot_avg_power","has_power_artifacts","artifact_count"])
    def collect(group_name: str, filt) -> dict[str, str]:
        subset = [r for r in trend if filt(r)]
        p_ratios = [float(r["gpu_total_power_ratio"]) for r in subset if r["gpu_total_power_ratio"]]
        e_ratios = [float(r["derived_energy_ratio"]) for r in subset if r["derived_energy_ratio"]]
        return {
            "group": group_name, "workload_count": str(len(subset)), "power_ratio_count": str(len(p_ratios)), "energy_ratio_count": str(len(e_ratios)),
            "arithmetic_mean_gpu_total_power_ratio": fmt(statistics.fmean(p_ratios) if p_ratios else None),
            "geomean_derived_energy_ratio": fmt(geomean(e_ratios)),
            "mean_derived_energy_saving": fmt(1 - statistics.fmean(e_ratios) if e_ratios else None),
        }
    groups = [
        collect("all", lambda r: True),
        collect("memory_rows", lambda r: r["paper_type"] == "M"),
        collect("compute_rows", lambda r: r["paper_type"] == "C"),
        collect("explicit_pass", lambda r: r["correctness_status"] == "explicit_pass"),
        collect("stats_only", lambda r: r["correctness_status"] != "explicit_pass"),
    ]
    write_csv(RESULTS / "w25f_energy_ratio_summary.csv", groups, ["group","workload_count","power_ratio_count","energy_ratio_count","arithmetic_mean_gpu_total_power_ratio","geomean_derived_energy_ratio","mean_derived_energy_saving"])
    power_rows = sum(1 for r in rows if r.get("kernel_avg_power") and r.get("gpu_tot_avg_power"))
    all_group = groups[0]
    md = ["# W25F Energy Trend Summary", "", f"actual_rows={len(rows)}", f"rows_with_kernel_and_gpu_power={power_rows}", f"frequency_source={FREQ_SOURCE}", "", "## Ratio summary", ""]
    for g in groups:
        md.append(f"- {g['group']}: power_ratio_mean={g['arithmetic_mean_gpu_total_power_ratio']}, derived_energy_geomean={g['geomean_derived_energy_ratio']}, mean_saving={g['mean_derived_energy_saving']}")
    md.append("\nCaveat: current-simulator AccelWattch trend only; not paper GPUWattch/GTX480 energy-saving reproduction.")
    (RESULTS / "w25f_energy_trend_summary.md").write_text("\n".join(md)+"\n", encoding="utf-8")
    DOC.write_text(f"""# Mascar W25F Energy Rerun Trend Report

## Executive summary

W25F reran the W25 selected subset after W25E runner/collector recovery. Actual rows: {len(rows)}. Rows with `kernel_avg_power` and `gpu_tot_avg_power`: {power_rows}/{len(rows)}.

## W25 issue and W25E fix recap

W25 completed runs but missed power fields. W25E showed power reports were generated outside collector-visible paths and fixed common runner artifact recovery plus recursive collector scanning.

## W25F rerun scope

Workloads: {', '.join(workloads)}. Configs: `{BASE}` and `{M4}`.

## Power artifact recovery results

All actual rows have power artifact directories and parsed power fields according to `w25f_power_artifact_check.csv` and `w25f_energy_availability_matrix.csv`.

## Energy/power field availability

`kernel_avg_power` and `gpu_tot_avg_power` are available for {power_rows}/{len(rows)} rows.

## Baseline vs M4 power table

See `w25f_energy_trend_results.csv`.

## Derived energy method

Direct energy fields were not used. Derived energy uses `gpu_tot_avg_power * runtime_seconds`; runtime_seconds is `gpu_tot_sim_cycle / 1132e6`, using the QV100 config clock: `{FREQ_SOURCE}`.

## Derived energy trend table

See `w25f_energy_ratio_summary.csv`. All-workload arithmetic mean GPU total power ratio is {all_group['arithmetic_mean_gpu_total_power_ratio']}; all-workload geomean derived energy ratio is {all_group['geomean_derived_energy_ratio']}.

## Caveats

This is a current-simulator AccelWattch/power trend. It is not the paper GPUWattch/GTX480 result and does not claim the paper's 12% energy saving. `completed_stats_found` and `completed_no_explicit_pass` are not correctness passes.

## Recommendations

Use W25E/W25F common runner artifact recovery for future energy sweeps, keep raw logs archived under `/workspace/tmp`, and report current-simulator trends separately from original-paper absolute reproduction.
""", encoding="utf-8")
    end_ts = int(time.time())
    branch = subprocess.check_output(["git","branch","--show-current"], cwd=ROOT, text=True).strip()
    head = subprocess.check_output(["git","rev-parse","--short","HEAD"], cwd=ROOT, text=True).strip()
    (AUDIT / "w25f_c_postcheck.md").write_text(f"""# W25F-C postcheck

start_ts={start_ts}
end_ts={end_ts}
elapsed_sec={end_ts-start_ts}
branch={branch}
HEAD={head}

actual_rows={len(rows)}
rows_with_kernel_avg_power={sum(1 for r in rows if r.get('kernel_avg_power'))}
rows_with_gpu_tot_avg_power={sum(1 for r in rows if r.get('gpu_tot_avg_power'))}
derived_energy_method=gpu_tot_avg_power_times_cycles_over_1132MHz
all_geomean_derived_energy_ratio={all_group['geomean_derived_energy_ratio']}
""", encoding="utf-8")
    print(f"w25f_analysis rows={len(rows)} power_rows={power_rows} geomean_energy={all_group['geomean_derived_energy_ratio']} mean_power={all_group['arithmetic_mean_gpu_total_power_ratio']}")
    return 0

if __name__ == "__main__":
    raise SystemExit(main())
