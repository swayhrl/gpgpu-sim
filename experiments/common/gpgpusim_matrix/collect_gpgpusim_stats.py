#!/usr/bin/env python3
"""Collect GPGPU-Sim matrix run stats and status classifications."""

from __future__ import annotations

import argparse
import csv
import re
from collections import Counter, defaultdict
from pathlib import Path


STAT_PATTERNS = {
    "gpu_tot_sim_cycle": re.compile(r"\bgpu_tot_sim_cycle\s*=\s*([0-9.]+)"),
    "gpu_tot_ipc": re.compile(r"\bgpu_tot_ipc\s*=\s*([0-9.]+)"),
    "gpu_tot_sim_insn": re.compile(r"\bgpu_tot_sim_insn\s*=\s*([0-9.]+)"),
    "gpu_sim_cycle": re.compile(r"\bgpu_sim_cycle\s*=\s*([0-9.]+)"),
    "gpu_sim_insn": re.compile(r"\bgpu_sim_insn\s*=\s*([0-9.]+)"),
    "paper_mascar_l1_sat_sample": re.compile(r"\bpaper_mascar_l1_sat_sample\s*=\s*([0-9.]+)"),
    "paper_mascar_l1_sat_sample_saturated": re.compile(r"\bpaper_mascar_l1_sat_sample_saturated\s*=\s*([0-9.]+)"),
    "paper_mascar_m2_ep_cycles": re.compile(r"\bpaper_mascar_m2_ep_cycles\s*=\s*([0-9.]+)"),
    "paper_mascar_m2_mp_cycles": re.compile(r"\bpaper_mascar_m2_mp_cycles\s*=\s*([0-9.]+)"),
    "paper_mascar_m2_owner_acquire": re.compile(r"\bpaper_mascar_m2_owner_acquire\s*=\s*([0-9.]+)"),
    "paper_mascar_m2_nonowner_mem_block": re.compile(r"\bpaper_mascar_m2_nonowner_mem_block\s*=\s*([0-9.]+)"),
    "paper_mascar_m3_hitonly_access_attempt": re.compile(r"\bpaper_mascar_m3_hitonly_access_attempt\s*=\s*([0-9.]+)"),
    "paper_mascar_m3_hitonly_access_hit": re.compile(r"\bpaper_mascar_m3_hitonly_access_hit\s*=\s*([0-9.]+)"),
    "paper_mascar_m3_hitonly_access_nack": re.compile(r"\bpaper_mascar_m3_hitonly_access_nack\s*=\s*([0-9.]+)"),
    "paper_mascar_m3_probe_attempt": re.compile(r"\bpaper_mascar_m3_probe_attempt\s*=\s*([0-9.]+)"),
    "paper_mascar_m3_probe_hit": re.compile(r"\bpaper_mascar_m3_probe_hit\s*=\s*([0-9.]+)"),
    "paper_mascar_m3_probe_nack": re.compile(r"\bpaper_mascar_m3_probe_nack\s*=\s*([0-9.]+)"),
    "paper_mascar_m3_probe_skip_not_mp": re.compile(r"\bpaper_mascar_m3_probe_skip_not_mp\s*=\s*([0-9.]+)"),
    "paper_mascar_m3_probe_skip_no_owner": re.compile(r"\bpaper_mascar_m3_probe_skip_no_owner\s*=\s*([0-9.]+)"),
    "paper_mascar_m3_probe_skip_owner": re.compile(r"\bpaper_mascar_m3_probe_skip_owner\s*=\s*([0-9.]+)"),
    "paper_mascar_m3_probe_skip_nonload": re.compile(r"\bpaper_mascar_m3_probe_skip_nonload\s*=\s*([0-9.]+)"),
    "paper_mascar_m3_nonowner_lsu_probe_allowed": re.compile(r"\bpaper_mascar_m3_nonowner_lsu_probe_allowed\s*=\s*([0-9.]+)"),
    "paper_mascar_m3_nonowner_lsu_probe_block_nonload": re.compile(r"\bpaper_mascar_m3_nonowner_lsu_probe_block_nonload\s*=\s*([0-9.]+)"),
    "paper_mascar_m3_nack_guard_owner_release": re.compile(r"\bpaper_mascar_m3_nack_guard_owner_release\s*=\s*([0-9.]+)"),
    "paper_mascar_m4_enqueue_success": re.compile(r"\bpaper_mascar_m4_enqueue_success\s*=\s*([0-9.]+)"),
    "paper_mascar_m4_retry_attempt": re.compile(r"\bpaper_mascar_m4_retry_attempt\s*=\s*([0-9.]+)"),
    "paper_mascar_m4_retry_hit": re.compile(r"\bpaper_mascar_m4_retry_hit\s*=\s*([0-9.]+)"),
    "paper_mascar_m4_retry_nack": re.compile(r"\bpaper_mascar_m4_retry_nack\s*=\s*([0-9.]+)"),
    "paper_mascar_m4_retry_requeue": re.compile(r"\bpaper_mascar_m4_retry_requeue(?:_tail)?\s*=\s*([0-9.]+)"),
    "paper_mascar_m4_queue_occupancy_max": re.compile(r"\bpaper_mascar_m4_(?:queue_occupancy_max|active_queue_max_occupancy)\s*=\s*([0-9.]+)"),
    "cacheinst_L1D_access_total": re.compile(r"\bcacheinst_L1D_access_total\s*=\s*([0-9.]+)"),
    "cacheinst_L1D_hit": re.compile(r"\bcacheinst_L1D_hit\s*=\s*([0-9.]+)"),
    "cacheinst_L1D_miss": re.compile(r"\bcacheinst_L1D_miss\s*=\s*([0-9.]+)"),
    "cacheinst_L1D_reservation_fail": re.compile(r"\bcacheinst_L1D_reservation_fail\s*=\s*([0-9.]+)"),
    "gpgpusim_exit": re.compile(r"\bgpgpusim_exit\s*=\s*([0-9.]+)"),
    "result_status": re.compile(r"\bresult_status\s*=\s*([A-Za-z0-9_./-]+)"),
    "result_pass": re.compile(r"\bresult_pass\s*=\s*([0-9.]+)"),
}

CRASH_RE = re.compile(r"Assertion|assert|SIGSEGV|segmentation fault|Aborted|core dumped", re.IGNORECASE)
PASS_RE = re.compile(r"(?m)^\s*(?:.*\bPASS\b.*|.*\bPASSED\b.*|.*Test Passed.*|.*passed verification.*)$")
MISSING_BINARY_RE = re.compile(r"No such file|binary missing|command not found|not executable", re.IGNORECASE)
MISSING_DATA_RE = re.compile(r"data.*missing|input.*missing|No such file.*data|cannot open.*input", re.IGNORECASE)

RESULT_FIELDS = [
    "run_id",
    "config_id",
    "paper_id",
    "paper_name",
    "paper_type",
    "wrapper_status",
    "run_dir",
    "exit_code",
    "timeout",
    "result_status_prelim",
    "classification",
    "has_stats",
    "explicit_pass",
    "gpu_tot_sim_cycle",
    "gpu_tot_ipc",
    "gpu_tot_sim_insn",
    "paper_mascar_l1_sat_sample",
    "paper_mascar_l1_sat_sample_saturated",
    "paper_mascar_m2_ep_cycles",
    "paper_mascar_m2_mp_cycles",
    "paper_mascar_m2_owner_acquire",
    "paper_mascar_m2_nonowner_mem_block",
    "paper_mascar_m3_hitonly_access_attempt",
    "paper_mascar_m3_hitonly_access_hit",
    "paper_mascar_m3_hitonly_access_nack",
    "paper_mascar_m3_probe_attempt",
    "paper_mascar_m3_probe_hit",
    "paper_mascar_m3_probe_nack",
    "paper_mascar_m3_probe_skip_not_mp",
    "paper_mascar_m3_probe_skip_no_owner",
    "paper_mascar_m3_probe_skip_owner",
    "paper_mascar_m3_probe_skip_nonload",
    "paper_mascar_m3_nonowner_lsu_probe_allowed",
    "paper_mascar_m3_nonowner_lsu_probe_block_nonload",
    "paper_mascar_m3_nack_guard_owner_release",
    "paper_mascar_m4_enqueue_success",
    "paper_mascar_m4_retry_attempt",
    "paper_mascar_m4_retry_hit",
    "paper_mascar_m4_retry_nack",
    "paper_mascar_m4_retry_requeue",
    "paper_mascar_m4_queue_occupancy_max",
    "cacheinst_L1D_access_total",
    "cacheinst_L1D_hit",
    "cacheinst_L1D_miss",
    "cacheinst_L1D_reservation_fail",
    "gpgpusim_exit",
    "result_status",
    "result_pass",
]


def read_csv(path: Path) -> list[dict[str, str]]:
    with path.open(newline="") as f:
        return list(csv.DictReader(f))


def write_csv(path: Path, rows: list[dict[str, str]], fields: list[str]) -> None:
    with path.open("w", newline="") as f:
        writer = csv.DictWriter(f, fieldnames=fields, lineterminator="\n")
        writer.writeheader()
        writer.writerows(rows)


def collect_log_text(run_dir: Path) -> str:
    parts: list[str] = []
    for name in ["combined.log", "stdout.log", "stderr.log", "stats.txt"]:
        p = run_dir / name
        if p.exists():
            parts.append(p.read_text(errors="replace"))
    for p in sorted(run_dir.glob("*.log")):
        if p.name not in {"combined.log", "stdout.log", "stderr.log"}:
            parts.append(p.read_text(errors="replace"))
    return "\n".join(parts)


def extract_stats(text: str) -> dict[str, str]:
    stats: dict[str, str] = {}
    for key, pattern in STAT_PATTERNS.items():
        match = pattern.search(text)
        stats[key] = match.group(1) if match else ""
    return stats


def has_core_stats(stats: dict[str, str]) -> bool:
    return bool(stats.get("gpu_tot_sim_cycle") or stats.get("gpu_sim_cycle"))


def classify(row: dict[str, str], text: str, stats: dict[str, str]) -> tuple[str, bool, bool]:
    exit_code = row.get("exit_code", "")
    timeout = row.get("timeout", "")
    prelim = row.get("result_status_prelim", "")
    wrapper_status = row.get("wrapper_status", "")
    has_stats = has_core_stats(stats)
    explicit_pass = bool(PASS_RE.search(text))

    if exit_code == "77" or prelim == "wrapper_unavailable":
        if wrapper_status == "placeholder_phase_unknown":
            return "phase_unknown", has_stats, explicit_pass
        if wrapper_status == "placeholder_missing_binary":
            return "missing_binary", has_stats, explicit_pass
        if wrapper_status == "placeholder_missing_source":
            return "missing_source", has_stats, explicit_pass
        return "wrapper_unavailable", has_stats, explicit_pass
    if timeout == "1" or prelim == "timeout":
        return "timeout", has_stats, explicit_pass
    if MISSING_DATA_RE.search(text):
        return "missing_data", has_stats, explicit_pass
    if MISSING_BINARY_RE.search(text):
        return "missing_binary", has_stats, explicit_pass
    if CRASH_RE.search(text):
        return "crash_assert", has_stats, explicit_pass
    if explicit_pass and exit_code == "0":
        return "completed_explicit_pass", has_stats, explicit_pass
    if has_stats and exit_code not in {"", "0"}:
        return "completed_nonzero_with_stats", has_stats, explicit_pass
    if has_stats and exit_code == "0":
        if stats.get("result_status") == "completed_no_explicit_pass":
            return "completed_no_explicit_pass", has_stats, explicit_pass
        return "completed_stats_found", has_stats, explicit_pass
    if exit_code not in {"", "0"}:
        return "simulator_error_no_stats", has_stats, explicit_pass
    if exit_code == "0":
        return "no_stats_exit0", has_stats, explicit_pass
    return "unknown", has_stats, explicit_pass


def summarize(rows: list[dict[str, str]], out: Path) -> None:
    counts = Counter(row["classification"] for row in rows)
    config_counts: dict[str, Counter[str]] = defaultdict(Counter)
    for row in rows:
        config_counts[row["config_id"]][row["classification"]] += 1

    with out.open("w") as f:
        f.write("# GPGPU-Sim Matrix Summary\n\n")
        f.write(f"Rows: {len(rows)}\n\n")
        f.write("## Classification Counts\n\n")
        for key, value in sorted(counts.items()):
            f.write(f"- {key}: {value}\n")
        f.write("\n## By Config\n\n")
        for config_id in sorted(config_counts):
            f.write(f"### {config_id}\n\n")
            for key, value in sorted(config_counts[config_id].items()):
                f.write(f"- {key}: {value}\n")
            f.write("\n")


def write_status_matrix(rows: list[dict[str, str]], out: Path) -> None:
    configs = sorted({row["config_id"] for row in rows})
    workloads = []
    seen = set()
    for row in rows:
        pid = row["paper_id"]
        if pid not in seen:
            workloads.append((pid, row["paper_name"], row["paper_type"]))
            seen.add(pid)
    by_pair = {(row["paper_id"], row["config_id"]): row["classification"] for row in rows}
    fields = ["paper_id", "paper_name", "paper_type", *configs]
    out_rows = []
    for paper_id, paper_name, paper_type in workloads:
        out_rows.append(
            {
                "paper_id": paper_id,
                "paper_name": paper_name,
                "paper_type": paper_type,
                **{config_id: by_pair.get((paper_id, config_id), "") for config_id in configs},
            }
        )
    write_csv(out, out_rows, fields)


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("run_outdir", type=Path)
    args = parser.parse_args()

    manifest = args.run_outdir / "run_manifest.csv"
    if not manifest.exists():
        raise SystemExit(f"missing run_manifest.csv under {args.run_outdir}")
    manifest_rows = read_csv(manifest)
    results = []
    for row in manifest_rows:
        run_dir = Path(row["run_dir"])
        text = collect_log_text(run_dir)
        stats = extract_stats(text)
        classification, has_stats, explicit_pass = classify(row, text, stats)
        result = {field: "" for field in RESULT_FIELDS}
        for key in [
            "run_id",
            "config_id",
            "paper_id",
            "paper_name",
            "paper_type",
            "wrapper_status",
            "run_dir",
            "exit_code",
            "timeout",
            "result_status_prelim",
        ]:
            result[key] = row.get(key, "")
        result["classification"] = classification
        result["has_stats"] = "1" if has_stats else "0"
        result["explicit_pass"] = "1" if explicit_pass else "0"
        for key in STAT_PATTERNS:
            if key in result:
                result[key] = stats.get(key, "")
        results.append(result)

    write_csv(args.run_outdir / "results.csv", results, RESULT_FIELDS)
    write_status_matrix(results, args.run_outdir / "status_matrix.csv")
    summarize(results, args.run_outdir / "summary.md")
    print(f"rows={len(results)}")
    print(f"results={args.run_outdir / 'results.csv'}")
    print(f"summary={args.run_outdir / 'summary.md'}")
    print(f"status_matrix={args.run_outdir / 'status_matrix.csv'}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
