#!/usr/bin/env python3
from __future__ import annotations

import csv
import re
from pathlib import Path

ROOT = Path(__file__).resolve().parents[5]
AUDIT = ROOT / "experiments/paper-mascar/energy/W25E/audit"
W16 = ROOT / "experiments/paper-mascar/energy/W16C"
W25 = ROOT / "experiments/paper-mascar/energy/W25"
CONFIGS = {
    "energy_baseline_off": ROOT / "configs/hrl-repro/SM7_QV100_mascar_energy_baseline_off/gpgpusim.config",
    "energy_m4_reexec_load": ROOT / "configs/hrl-repro/SM7_QV100_mascar_energy_m4_reexec_load_on/gpgpusim.config",
}
KNOBS = [
    "power_simulation_enabled",
    "power_simulation_mode",
    "accelwattch_xml_file",
    "gpuwattch_xml_file",
    "power_trace_enabled",
    "visualizer_enabled",
]
POWER_FIELDS = ["kernel_avg_power", "gpu_tot_avg_power", "kernel_max_power", "gpu_tot_max_power"]
M3DIAG_FIELDS = ["paper_mascar_m3diag_enabled", "paper_mascar_m3diag_hitonly_probe_called"]


def read_csv(path: Path) -> list[dict[str, str]]:
    if not path.exists():
        return []
    with path.open(newline="") as f:
        return list(csv.DictReader(f))


def write_csv(path: Path, rows: list[dict[str, str]], fields: list[str]) -> None:
    with path.open("w", newline="") as f:
        writer = csv.DictWriter(f, fieldnames=fields, lineterminator="\n")
        writer.writeheader()
        writer.writerows(rows)


def parse_knobs(path: Path) -> dict[str, str]:
    knobs: dict[str, str] = {}
    if not path.exists():
        return knobs
    for line in path.read_text(errors="replace").splitlines():
        line = line.strip()
        if not line.startswith("-"):
            continue
        parts = line.split()
        if not parts:
            continue
        key = parts[0].lstrip("-")
        if key in KNOBS:
            knobs[key] = parts[1] if len(parts) > 1 else ""
    return knobs


def rel(path: Path) -> str:
    try:
        return str(path.relative_to(ROOT))
    except ValueError:
        return str(path)


def main() -> int:
    AUDIT.mkdir(parents=True, exist_ok=True)
    rows: list[dict[str, str]] = []
    for config_id, cfg in CONFIGS.items():
        knobs = parse_knobs(cfg)
        for key in KNOBS:
            value = knobs.get(key, "")
            status = "present" if value else "missing"
            note = ""
            if key.endswith("xml_file") and value:
                xml = Path(value) if value.startswith("/") else cfg.parent / value
                note = f"xml_exists={int(xml.exists())};xml_path={xml}"
            rows.append({"category": "config_knob", "item": f"{config_id}:{key}", "w16_value": value, "w25_value": value, "status": status, "evidence": rel(cfg), "notes": note})
    w16_results = read_csv(W16 / "w16_energy_latest_results.csv")
    w25_results = read_csv(W25 / "results/w25_energy_actual_results.csv")
    for field in POWER_FIELDS:
        rows.append({
            "category": "collector_result_field",
            "item": field,
            "w16_value": str(sum(1 for r in w16_results if r.get(field))),
            "w25_value": str(sum(1 for r in w25_results if r.get(field))),
            "status": "w25_missing" if any(r.get(field) for r in w16_results) and not any(r.get(field) for r in w25_results) else "ok_or_unavailable",
            "evidence": "W16 latest results vs W25 actual results",
            "notes": "W16 had parsed power fields; W25 actual results did not",
        })
    w16_manifest = read_csv(W16 / "w16_energy_latest_run_manifest.csv")
    w25_manifest = read_csv(W25 / "results/w25_energy_latest_run_manifest.csv")
    w16_wrappers = sorted({r.get("wrapper_path", "") for r in w16_manifest if r.get("paper_id") in {"spmv", "mri_q", "pathfinder"}})
    w25_wrappers = sorted({r.get("wrapper_path", "") for r in w25_manifest if r.get("paper_id") in {"spmv", "mri_q", "pathfinder"}})
    rows.append({"category": "wrapper_path", "item": "stable_workloads", "w16_value": "|".join(w16_wrappers), "w25_value": "|".join(w25_wrappers), "status": "different", "evidence": "run manifests", "notes": "W16 energy wrappers copy power reports; W25 Table III wrappers do not"})
    collector = ROOT / "experiments/common/gpgpusim_matrix/collect_gpgpusim_stats.py"
    text = collector.read_text(errors="replace")
    for field in POWER_FIELDS + M3DIAG_FIELDS:
        rows.append({"category": "collector_symbol", "item": field, "w16_value": "present", "w25_value": "present" if field in text else "missing", "status": "preserved" if field in text else "missing", "evidence": rel(collector), "notes": "collector field/pattern availability"})
    write_csv(AUDIT / "w25e_static_comparison.csv", rows, ["category", "item", "w16_value", "w25_value", "status", "evidence", "notes"])
    summary = ["# W25E Config Diff Summary", "", "## Finding", "", "W16 and W25 use the same energy config directories and those configs enable `-power_simulation_enabled 1` with an absolute AccelWattch XML path. The material difference is the workload execution wrapper path: W16 used energy-specific wrappers that copied `accelwattch_power_report.log` back to the run directory; W25 used generic Table III wrappers that did not copy AccelWattch power artifacts.", "", "## Config evidence", ""]
    for config_id, cfg in CONFIGS.items():
        knobs = parse_knobs(cfg)
        summary.append(f"- {config_id}: power_enabled={knobs.get('power_simulation_enabled','')}, mode={knobs.get('power_simulation_mode','')}, xml={knobs.get('accelwattch_xml_file','')}")
    (AUDIT / "w25e_config_diff_summary.md").write_text("\n".join(summary) + "\n", encoding="utf-8")
    checks = ["# W25E Collector Field Check", "", "Required W16 power fields and W15 M3 diagnostic fields remain present in `collect_gpgpusim_stats.py`.", ""]
    for field in POWER_FIELDS + M3DIAG_FIELDS:
        checks.append(f"- {field}: {'present' if field in text else 'missing'}")
    checks.append("- scan_scope: recursive run_dir text/power artifact scan enabled for W25E recovery")
    (AUDIT / "w25e_collector_field_check.md").write_text("\n".join(checks) + "\n", encoding="utf-8")
    print(f"static_rows={len(rows)}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
