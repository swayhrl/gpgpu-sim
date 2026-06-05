#!/usr/bin/env python3
"""Prepare W9-W14 workload manifests and config matrices."""

from __future__ import annotations

import csv
from pathlib import Path


REPO = Path(__file__).resolve().parents[5]
BASE = REPO / "experiments" / "paper-mascar" / "workloads"
MATRIX = BASE / "matrix"

TABLE = BASE / "mascar_table_iii_command_manifest.csv"
W7 = MATRIX / "W7" / "w7_iter1_command_manifest.csv"
W5C = MATRIX / "W5C" / "w5c_iter2_command_manifest.csv"

CONFIG_ROWS = [
    {
        "config_id": "baseline_off",
        "config_path": "configs/hrl-repro/SM7_QV100_mascar_baseline_off",
        "config_role": "baseline",
        "enabled": "1",
        "config_notes": "Mascar disabled baseline.",
    },
    {
        "config_id": "m2_owner_sched",
        "config_path": "configs/hrl-repro/SM7_QV100_mascar_m2_owner_sched_on",
        "config_role": "m2_active",
        "enabled": "1",
        "config_notes": "M2 owner scheduling active.",
    },
    {
        "config_id": "m3_hitonly_nack",
        "config_path": "configs/hrl-repro/SM7_QV100_mascar_m3_hitonly_nack_on",
        "config_role": "m3_active",
        "enabled": "1",
        "config_notes": "M3 non-owner hit-only/NACK active.",
    },
    {
        "config_id": "m4_reexec_load",
        "config_path": "configs/hrl-repro/SM7_QV100_mascar_m4_reexec_load_on",
        "config_role": "m4_active",
        "enabled": "1",
        "config_notes": "M4 load re-exec active.",
    },
]

DIRECT_RUNS = {
    "bp_1": ("/workspace/repos/gpgpu-workloads/suites/rodinia-wrapper/backprop", "./backprop 256", "direct_backprop_256"),
    "bp_2": ("/workspace/repos/gpgpu-workloads/suites/rodinia-wrapper/backprop", "./backprop 256", "direct_backprop_256"),
    "bfs": ("/workspace/repos/gpgpu-workloads/suites/rodinia-wrapper/bfs", "./bfs.out data/tiny_graph.txt", "direct_bfs_tiny"),
    "histo_1": ("/workspace/repos/gpgpu-workloads/suites/parboil-wrapper/histo", "./histo 1 -i data/tiny.bin", "direct_histo_tiny"),
    "histo_2": ("/workspace/repos/gpgpu-workloads/suites/parboil-wrapper/histo", "./histo 1 -i data/tiny.bin", "direct_histo_tiny"),
    "histo_3": ("/workspace/repos/gpgpu-workloads/suites/parboil-wrapper/histo", "./histo 1 -i data/tiny.bin", "direct_histo_tiny"),
    "kmeans_1": ("/workspace/repos/gpgpu-workloads/suites/rodinia-wrapper/kmeans", "./kmeans -i data/tiny.txt -n 4 -m 4", "direct_kmeans_tiny"),
    "kmeans_2": ("/workspace/repos/gpgpu-workloads/suites/rodinia-wrapper/kmeans", "./kmeans -i data/tiny.txt -n 4 -m 4", "direct_kmeans_tiny"),
    "mri_q": ("/workspace/repos/gpgpu-workloads/suites/parboil-wrapper/mri-q", "./mri-q -i data/tiny.bin 32", "direct_mriq_tiny"),
    "pathfinder": ("/workspace/repos/gpgpu-workloads/suites/rodinia-wrapper/pathfinder", "./pathfinder 128 128 4", "direct_pathfinder_128x128_p4"),
    "sgemm": ("/workspace/repos/gpgpu-workloads/suites/parboil-wrapper/sgemm", "bash run_gpgpusim.sh", "direct_sgemm_tiny"),
    "spmv": ("/workspace/repos/gpgpu-workloads/suites/parboil-wrapper/spmv", "bash run_gpgpusim.sh", "direct_spmv_tiny"),
    "srad_1": ("/workspace/repos/gpgpu-workloads/suites/rodinia-wrapper/srad_v2", "./srad 64 64 0 31 0 31 0.5 1", "direct_srad_tiny"),
    "srad_2": ("/workspace/repos/gpgpu-workloads/suites/rodinia-wrapper/srad_v2", "./srad 64 64 0 31 0 31 0.5 1", "direct_srad_tiny"),
    "stencil": ("/workspace/repos/gpgpu-workloads/suites/parboil-wrapper/stencil", "./stencil -i data/tiny.bin 8 8 8 1", "direct_stencil_tiny"),
}


def read_csv(path: Path) -> list[dict[str, str]]:
    with path.open(newline="") as f:
        return list(csv.DictReader(f))


def write_csv(path: Path, rows: list[dict[str, str]], fields: list[str]) -> None:
    path.parent.mkdir(parents=True, exist_ok=True)
    with path.open("w", newline="") as f:
        writer = csv.DictWriter(f, fieldnames=fields, lineterminator="\n")
        writer.writeheader()
        writer.writerows([{field: row.get(field, "") for field in fields} for row in rows])


def config_matrix(path: Path, ids: set[str]) -> None:
    rows = []
    for row in CONFIG_ROWS:
        copy = dict(row)
        copy["enabled"] = "1" if row["config_id"] in ids else "0"
        rows.append(copy)
    write_csv(path, rows, ["config_id", "config_path", "config_role", "enabled", "config_notes"])


def merge_attempt(table: list[dict[str, str]], attempt_rows: list[dict[str, str]], attempt: str) -> list[dict[str, str]]:
    by_id = {row["paper_id"]: row for row in attempt_rows}
    targets = {"spmv", "mri_q", "pathfinder"}
    out = []
    for row in table:
        copy = dict(row)
        if row["paper_id"] in targets and row["paper_id"] in by_id:
            src = by_id[row["paper_id"]]
            for key in [
                "availability_status",
                "wrapper_path",
                "wrapper_status",
                "build_required",
                "build_command",
                "run_working_dir",
                "run_command",
                "input_size",
                "timeout_sec",
                "dry_run_status",
            ]:
                copy[key] = src.get(key, copy.get(key, ""))
            copy["timeout_sec"] = "900"
            copy["notes"] = f"W9 {attempt}: {src.get('notes', '')}"
        else:
            copy["wrapper_status"] = "placeholder_out_of_scope"
            copy["notes"] = f"W9 {attempt}: preserved Table III row outside W7 activation-ready subset"
        out.append(copy)
    return out


def w9_attempt3(table: list[dict[str, str]]) -> list[dict[str, str]]:
    rows = merge_attempt(table, read_csv(W7), "attempt3_pathfinder_512")
    for row in rows:
        if row["paper_id"] == "pathfinder":
            row["run_command"] = "./pathfinder 512 512 4"
            row["input_size"] = "w9_attempt3_512x512_p4"
            row["timeout_sec"] = "1200"
            row["notes"] = (
                "W9 attempt3: pathfinder dimensions doubled again to increase "
                "concurrent memory pressure and search for M3 non-owner hit-only activation"
            )
        elif row["paper_id"] in {"spmv", "mri_q"}:
            row["timeout_sec"] = "1200"
            row["notes"] = f"W9 attempt3: retained W7 large input as control; {row.get('notes', '')}"
    return rows


def expanded_ready(table: list[dict[str, str]]) -> list[dict[str, str]]:
    out = []
    for row in table:
        copy = dict(row)
        if (
            row.get("wrapper_status") == "placeholder_phase_unknown"
            and row.get("run_working_dir")
            and row.get("run_command")
        ):
            copy["availability_status"] = "available_approx_phase"
            copy["wrapper_status"] = "ready"
            copy["timeout_sec"] = "600"
            copy["notes"] = f"W10 approximate phase expansion; original note: {row.get('notes', '')}"
        elif row.get("wrapper_status") == "ready":
            copy["timeout_sec"] = "600"
            copy["notes"] = f"W10 retained ready row; original note: {row.get('notes', '')}"
        if copy.get("wrapper_status") == "ready" and copy["paper_id"] in DIRECT_RUNS:
            run_dir, command, input_size = DIRECT_RUNS[copy["paper_id"]]
            copy["run_working_dir"] = run_dir
            copy["run_command"] = command
            copy["input_size"] = input_size
            copy["notes"] = f"{copy.get('notes', '')}; W10 direct command to keep matrix config override restorable"
        out.append(copy)
    return out


def typed_manifest(rows: list[dict[str, str]], paper_type: str, stage: str) -> list[dict[str, str]]:
    out = []
    for row in rows:
        copy = dict(row)
        if row.get("paper_type") != paper_type or row.get("wrapper_status") != "ready":
            copy["wrapper_status"] = "placeholder_out_of_scope"
            copy["availability_status"] = copy.get("availability_status", "") or "out_of_scope"
            copy["notes"] = f"{stage}: preserved Table III row outside active {paper_type} sweep"
        else:
            copy["notes"] = f"{stage}: selected ready {paper_type} workload; {copy.get('notes', '')}"
        out.append(copy)
    return out


def main() -> int:
    table = read_csv(TABLE)
    fields = list(table[0].keys())
    w7 = read_csv(W7)
    w5c = read_csv(W5C)

    w9 = MATRIX / "W9"
    write_csv(w9 / "w9_attempt1_command_manifest.csv", merge_attempt(table, w7, "attempt1_w7_large"), fields)
    write_csv(w9 / "w9_attempt2_command_manifest.csv", merge_attempt(table, w5c, "attempt2_w5c_bounded"), fields)
    write_csv(w9 / "w9_attempt3_command_manifest.csv", w9_attempt3(table), fields)
    config_matrix(w9 / "w9_config_matrix.csv", {"m3_hitonly_nack", "m4_reexec_load"})

    w10_rows = expanded_ready(table)
    w10 = MATRIX / "W10"
    write_csv(w10 / "w10_workload_manifest_ready.csv", w10_rows, fields)
    config_matrix(w10 / "w10_config_matrix.csv", {"baseline_off"})

    w11 = MATRIX / "W11"
    write_csv(w11 / "w11_memory_workload_manifest.csv", typed_manifest(w10_rows, "M", "W11"), fields)
    config_matrix(w11 / "w11_config_matrix.csv", {"baseline_off", "m2_owner_sched", "m3_hitonly_nack", "m4_reexec_load"})

    w12 = MATRIX / "W12"
    write_csv(w12 / "w12_compute_workload_manifest.csv", typed_manifest(w10_rows, "C", "W12"), fields)
    config_matrix(w12 / "w12_config_matrix.csv", {"baseline_off", "m2_owner_sched", "m3_hitonly_nack", "m4_reexec_load"})

    print("w9_attempt1_rows", len(read_csv(w9 / "w9_attempt1_command_manifest.csv")))
    print("w9_attempt2_rows", len(read_csv(w9 / "w9_attempt2_command_manifest.csv")))
    print("w10_rows", len(w10_rows), "ready", sum(1 for r in w10_rows if r.get("wrapper_status") == "ready"))
    print("w11_rows", len(read_csv(w11 / "w11_memory_workload_manifest.csv")))
    print("w12_rows", len(read_csv(w12 / "w12_compute_workload_manifest.csv")))
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
