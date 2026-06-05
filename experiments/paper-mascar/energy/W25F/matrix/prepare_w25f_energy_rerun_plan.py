#!/usr/bin/env python3
from __future__ import annotations

import csv
import re
import subprocess
import time
from pathlib import Path

ROOT = Path(__file__).resolve().parents[5]
W25F = ROOT / "experiments/paper-mascar/energy/W25F"
MATRIX = W25F / "matrix"
AUDIT = W25F / "audit"
DOC = ROOT / "docs/papers/mascar_w25f_energy_rerun_plan.md"
SELECTED = ["bp_2", "srad_1", "bp_1", "spmv", "mri_q", "pathfinder"]
CONFIGS = [
    ("energy_baseline_off", "configs/hrl-repro/SM7_QV100_mascar_energy_baseline_off", "energy_baseline"),
    ("energy_m4_reexec_load", "configs/hrl-repro/SM7_QV100_mascar_energy_m4_reexec_load_on", "energy_m4"),
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


def parse_knob(config: Path, knob: str) -> str:
    if not config.exists():
        return ""
    pat = re.compile(rf"^-{re.escape(knob)}\s+(\S+)")
    for line in config.read_text(errors="replace").splitlines():
        m = pat.match(line.strip())
        if m:
            return m.group(1)
    return ""


def xml_exists(config_dir: Path) -> tuple[bool, str]:
    cfg = config_dir / "gpgpusim.config"
    value = parse_knob(cfg, "accelwattch_xml_file")
    if not value:
        return False, "missing accelwattch_xml_file"
    path = Path(value) if value.startswith("/") else config_dir / value
    return path.exists(), str(path)


def main() -> int:
    MATRIX.mkdir(parents=True, exist_ok=True)
    AUDIT.mkdir(parents=True, exist_ok=True)
    start_ts = int(Path("/tmp/mascar_w25f_start_ts").read_text().strip()) if Path("/tmp/mascar_w25f_start_ts").exists() else int(time.time())
    w25_command_rows = read_csv(ROOT / "experiments/paper-mascar/energy/W25/matrix/w25_energy_command_manifest.csv")
    if not w25_command_rows:
        raise SystemExit("missing W25 command manifest; cannot build W25F command manifest")
    by_id = {r["paper_id"]: r for r in w25_command_rows}
    missing = [w for w in SELECTED if w not in by_id]
    if missing:
        raise SystemExit(f"missing selected workloads in W25 command manifest: {missing}")

    config_rows = []
    for cid, cpath, role in CONFIGS:
        config_rows.append({"config_id": cid, "config_path": cpath, "config_role": role, "enabled": "1", "notes": "W25F current-simulator energy rerun config"})
    write_csv(MATRIX / "w25f_energy_config_matrix.csv", config_rows, ["config_id", "config_path", "config_role", "enabled", "notes"])

    workload_rows = []
    command_rows = []
    reasons = {
        "bp_2": "W24 M2/M4 active memory row",
        "srad_1": "W24 M2/M4 active memory row",
        "bp_1": "W24 M2/M4 active compute row",
        "spmv": "W16/W25E energy-stable and W24 ready memory row",
        "mri_q": "W16 energy-stable and W24 ready compute row",
        "pathfinder": "W16 energy-stable and W24 ready compute row",
    }
    for wid in SELECTED:
        src = by_id[wid]
        workload_rows.append({
            "workload_id": wid,
            "paper_id": src.get("paper_id", wid),
            "paper_name": src.get("paper_name", wid),
            "paper_type": src.get("paper_type", ""),
            "wrapper_path": src.get("wrapper_path", ""),
            "wrapper_status": src.get("wrapper_status", ""),
            "selected_for_w25f": "1",
            "selection_reason": reasons[wid],
            "timeout_sec": src.get("timeout_sec", "1800") or "1800",
            "prior_w25_status": "completed_stats_found_or_explicit_pass_power_missing",
            "notes": "current-simulator rerun after W25E power artifact recovery; not paper GTX480 GPUWattch",
        })
        command_rows.append({
            "paper_id": src.get("paper_id", wid),
            "paper_name": src.get("paper_name", wid),
            "paper_type": src.get("paper_type", ""),
            "availability": src.get("availability", "available"),
            "wrapper_path": src.get("wrapper_path", ""),
            "wrapper_status": src.get("wrapper_status", "ready"),
            "build_required": src.get("build_required", "no"),
            "build_command": src.get("build_command", ""),
            "run_working_dir": src.get("run_working_dir", ""),
            "run_command": src.get("run_command", ""),
            "input_size": src.get("input_size", "tiny"),
            "timeout_sec": src.get("timeout_sec", "1800") or "1800",
            "dry_run_status": src.get("dry_run_status", "pass"),
            "notes": f"W25F rerun; {reasons[wid]}",
        })
    write_csv(MATRIX / "w25f_energy_workload_manifest.csv", workload_rows, ["workload_id", "paper_id", "paper_name", "paper_type", "wrapper_path", "wrapper_status", "selected_for_w25f", "selection_reason", "timeout_sec", "prior_w25_status", "notes"])
    write_csv(MATRIX / "w25f_energy_command_manifest.csv", command_rows, ["paper_id", "paper_name", "paper_type", "availability", "wrapper_path", "wrapper_status", "build_required", "build_command", "run_working_dir", "run_command", "input_size", "timeout_sec", "dry_run_status", "notes"])

    run_plan = []
    for cfg in config_rows:
        for wl in workload_rows:
            run_plan.append({
                "run_id": f"{cfg['config_id']}__{wl['workload_id']}",
                "config_id": cfg["config_id"],
                "config_path": cfg["config_path"],
                "config_role": cfg["config_role"],
                "workload_id": wl["workload_id"],
                "paper_id": wl["paper_id"],
                "wrapper_path": wl["wrapper_path"],
                "selected_for_run": "1",
                "timeout_sec": wl["timeout_sec"],
                "expected_power_artifacts": "accelwattch_power_report.log or power_artifacts/*power*",
                "notes": "bounded W25F current-simulator energy rerun",
            })
    write_csv(MATRIX / "w25f_energy_run_plan.csv", run_plan, ["run_id", "config_id", "config_path", "config_role", "workload_id", "paper_id", "wrapper_path", "selected_for_run", "timeout_sec", "expected_power_artifacts", "notes"])

    runner_text = (ROOT / "experiments/common/gpgpusim_matrix/run_gpgpusim_matrix.sh").read_text(errors="replace")
    collector_text = (ROOT / "experiments/common/gpgpusim_matrix/collect_gpgpusim_stats.py").read_text(errors="replace")
    preflight = []
    def add(check_id: str, item: str, ok: bool, evidence: str, notes: str = "") -> None:
        preflight.append({"check_id": check_id, "item": item, "status": "pass" if ok else "fail", "evidence": evidence, "notes": notes})
    add("runner_has_power_artifact_recovery", "copy_power_artifacts", "copy_power_artifacts" in runner_text and "power_artifacts" in runner_text, "experiments/common/gpgpusim_matrix/run_gpgpusim_matrix.sh")
    add("collector_has_recursive_scan", "run_dir.rglob", "run_dir.rglob" in collector_text, "experiments/common/gpgpusim_matrix/collect_gpgpusim_stats.py")
    add("collector_has_kernel_avg_power_pattern", "kernel_avg_power", "kernel_avg_power" in collector_text, "experiments/common/gpgpusim_matrix/collect_gpgpusim_stats.py")
    add("collector_has_gpu_tot_avg_power_pattern", "gpu_tot_avg_power", "gpu_tot_avg_power" in collector_text, "experiments/common/gpgpusim_matrix/collect_gpgpusim_stats.py")
    for cid, cpath, role in CONFIGS:
        cdir = ROOT / cpath
        xml_ok, xml_path = xml_exists(cdir)
        add(f"{cid}_config_exists", cid, cdir.exists(), cpath)
        add(f"{cid}_power_enabled", cid, parse_knob(cdir / "gpgpusim.config", "power_simulation_enabled") == "1", str(cdir / "gpgpusim.config"))
        add(f"accelwattch_xml_exists_{cid}", cid, xml_ok, xml_path)
    add("workload_manifest_rows", "selected workloads", len(workload_rows) == 6, str(MATRIX / "w25f_energy_workload_manifest.csv"), f"rows={len(workload_rows)}")
    add("run_plan_rows", "6 workloads x 2 configs", len(run_plan) == 12, str(MATRIX / "w25f_energy_run_plan.csv"), f"rows={len(run_plan)}")
    write_csv(MATRIX / "w25f_preflight_check.csv", preflight, ["check_id", "item", "status", "evidence", "notes"])

    DOC.write_text(f"""# Mascar W25F Energy Rerun Plan

## Goal

Rerun the W25 selected subset with W25E runner/collector recovery: 6 workloads x 2 energy configs = 12 actual rows.

## W25 issue summary

W25 completed 12/12 runs but collected zero true power fields because power artifacts were not available to the collector.

## W25E fix summary

W25E added common runner power artifact recovery into `run_dir/power_artifacts/` and bounded recursive collector scanning while preserving W15 M3 diagnostic and W16 power fields.

## Selected workloads

{', '.join(SELECTED)}

## Config matrix

- `energy_baseline_off`: `configs/hrl-repro/SM7_QV100_mascar_energy_baseline_off`
- `energy_m4_reexec_load`: `configs/hrl-repro/SM7_QV100_mascar_energy_m4_reexec_load_on`

## Preflight checks

Preflight rows: {len(preflight)}. Failing rows: {sum(1 for r in preflight if r['status'] != 'pass')}.

## Run plan

Run plan rows: {len(run_plan)}.

## Expected outputs

W25F-B will generate dry-run/actual results and power artifact checks. W25F-C will compute current-simulator power and derived-energy trend.

## Caveats

This is a current-simulator AccelWattch/power trend rerun, not paper GPUWattch/GTX480 energy reproduction and not a 12% paper energy-saving claim.
""", encoding="utf-8")
    end_ts = int(time.time())
    branch = subprocess.check_output(["git", "branch", "--show-current"], cwd=ROOT, text=True).strip()
    head = subprocess.check_output(["git", "rev-parse", "--short", "HEAD"], cwd=ROOT, text=True).strip()
    (AUDIT / "w25f_a_postcheck.md").write_text(f"""# W25F-A postcheck

start_ts={start_ts}
end_ts={end_ts}
elapsed_sec={end_ts-start_ts}
branch={branch}
HEAD={head}

workload_rows={len(workload_rows)}
config_rows={len(config_rows)}
run_plan_rows={len(run_plan)}
preflight_pass={sum(1 for r in preflight if r['status']=='pass')}/{len(preflight)}
""", encoding="utf-8")
    print(f"w25f_plan workloads={len(workload_rows)} configs={len(config_rows)} run_plan={len(run_plan)} preflight_pass={sum(1 for r in preflight if r['status']=='pass')}/{len(preflight)}")
    return 0

if __name__ == "__main__":
    raise SystemExit(main())
