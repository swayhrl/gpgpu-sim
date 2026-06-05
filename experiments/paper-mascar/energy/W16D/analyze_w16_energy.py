#!/usr/bin/env python3
"""Analyze W16 current-simulator energy trend from collected power fields."""

from __future__ import annotations

import csv
from pathlib import Path

ROOT = Path(__file__).resolve().parents[4]
W16C = ROOT / "experiments/paper-mascar/energy/W16C"
W16D = ROOT / "experiments/paper-mascar/energy/W16D"
RESULTS = W16C / "w16_energy_latest_results.csv"
CORE_CLOCK_HZ = 1132.0e6
BASELINE = "energy_baseline_off"
M4 = "energy_m4_reexec_load"


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
        writer.writerows(rows)


def fnum(value: str) -> float | None:
    try:
        if value == "":
            return None
        return float(value)
    except ValueError:
        return None


def ratio(num: float | None, den: float | None) -> str:
    if num is None or den in (None, 0):
        return ""
    return f"{num / den:.6g}"


def main() -> int:
    rows = read_csv(RESULTS)
    trend_rows: list[dict[str, object]] = []
    availability_rows: list[dict[str, object]] = []
    by_workload: dict[str, dict[str, dict[str, str]]] = {}

    for row in rows:
        cycles = fnum(row.get("gpu_tot_sim_cycle", ""))
        avg_power = fnum(row.get("power_total_avg", ""))
        peak_power = fnum(row.get("power_peak", ""))
        runtime_sec = cycles / CORE_CLOCK_HZ if cycles is not None else None
        est_energy = avg_power * runtime_sec if avg_power is not None and runtime_sec is not None else None
        row_key = row.get("paper_id", "")
        cfg = row.get("config_id", "")
        by_workload.setdefault(row_key, {})[cfg] = row
        trend_rows.append(
            {
                "paper_id": row_key,
                "paper_name": row.get("paper_name", ""),
                "config_id": cfg,
                "classification": row.get("classification", ""),
                "energy_fields_found": row.get("energy_fields_found", ""),
                "gpu_tot_sim_cycle": row.get("gpu_tot_sim_cycle", ""),
                "runtime_sec_est": "" if runtime_sec is None else f"{runtime_sec:.9g}",
                "power_total_avg_w": "" if avg_power is None else f"{avg_power:.9g}",
                "power_peak_w": "" if peak_power is None else f"{peak_power:.9g}",
                "energy_total_direct_j": row.get("energy_total", ""),
                "energy_total_est_j": "" if est_energy is None else f"{est_energy:.9g}",
                "energy_basis": "derived_avg_power_x_cycles" if est_energy is not None else "unavailable",
            }
        )
        availability_rows.append(
            {
                "paper_id": row_key,
                "config_id": cfg,
                "classification": row.get("classification", ""),
                "has_stats": row.get("has_stats", ""),
                "energy_fields_found": row.get("energy_fields_found", ""),
                "has_power_total_avg": "1" if avg_power is not None else "0",
                "has_direct_energy_total": "1" if row.get("energy_total", "") else "0",
            }
        )

    ratio_rows: list[dict[str, object]] = []
    for paper_id, cfg_rows in sorted(by_workload.items()):
        b = cfg_rows.get(BASELINE, {})
        m = cfg_rows.get(M4, {})
        b_cycles = fnum(b.get("gpu_tot_sim_cycle", ""))
        m_cycles = fnum(m.get("gpu_tot_sim_cycle", ""))
        b_power = fnum(b.get("power_total_avg", ""))
        m_power = fnum(m.get("power_total_avg", ""))
        b_energy = b_power * (b_cycles / CORE_CLOCK_HZ) if b_power is not None and b_cycles is not None else None
        m_energy = m_power * (m_cycles / CORE_CLOCK_HZ) if m_power is not None and m_cycles is not None else None
        ratio_rows.append(
            {
                "paper_id": paper_id,
                "paper_name": b.get("paper_name") or m.get("paper_name", ""),
                "baseline_classification": b.get("classification", ""),
                "m4_classification": m.get("classification", ""),
                "baseline_power_total_avg_w": "" if b_power is None else f"{b_power:.9g}",
                "m4_power_total_avg_w": "" if m_power is None else f"{m_power:.9g}",
                "power_avg_ratio_m4_over_baseline": ratio(m_power, b_power),
                "baseline_energy_total_est_j": "" if b_energy is None else f"{b_energy:.9g}",
                "m4_energy_total_est_j": "" if m_energy is None else f"{m_energy:.9g}",
                "energy_est_ratio_m4_over_baseline": ratio(m_energy, b_energy),
                "cycle_ratio_m4_over_baseline": ratio(m_cycles, b_cycles),
                "trend_available": "1" if b_energy is not None and m_energy is not None else "0",
            }
        )

    write_csv(W16D / "w16_energy_trend_summary.csv", trend_rows, [
        "paper_id", "paper_name", "config_id", "classification", "energy_fields_found",
        "gpu_tot_sim_cycle", "runtime_sec_est", "power_total_avg_w", "power_peak_w",
        "energy_total_direct_j", "energy_total_est_j", "energy_basis",
    ])
    write_csv(W16D / "w16_energy_ratio_by_workload.csv", ratio_rows, [
        "paper_id", "paper_name", "baseline_classification", "m4_classification",
        "baseline_power_total_avg_w", "m4_power_total_avg_w", "power_avg_ratio_m4_over_baseline",
        "baseline_energy_total_est_j", "m4_energy_total_est_j", "energy_est_ratio_m4_over_baseline",
        "cycle_ratio_m4_over_baseline", "trend_available",
    ])
    write_csv(W16D / "w16_energy_availability_matrix.csv", availability_rows, [
        "paper_id", "config_id", "classification", "has_stats", "energy_fields_found",
        "has_power_total_avg", "has_direct_energy_total",
    ])

    available = [r for r in ratio_rows if r["trend_available"] == "1"]
    with (W16D / "w16_energy_summary.md").open("w") as f:
        f.write("# W16 Energy Trend Summary\n\n")
        f.write("This report uses current-simulator AccelWattch power fields. Estimated energy is derived as average power times runtime from cycles at 1132 MHz. It is not a paper GPUWattch/GTX480 absolute energy reproduction.\n\n")
        f.write(f"Rows analyzed: {len(rows)}\n\n")
        f.write(f"Workloads with complete baseline/M4 derived trend: {len(available)}\n\n")
        f.write("## Ratio by Workload\n\n")
        for r in ratio_rows:
            f.write(f"- {r['paper_id']}: trend_available={r['trend_available']}, energy_est_ratio_m4_over_baseline={r['energy_est_ratio_m4_over_baseline']}, power_avg_ratio_m4_over_baseline={r['power_avg_ratio_m4_over_baseline']}, cycle_ratio_m4_over_baseline={r['cycle_ratio_m4_over_baseline']}\n")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
