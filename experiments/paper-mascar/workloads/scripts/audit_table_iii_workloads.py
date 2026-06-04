#!/usr/bin/env python3
"""Audit local availability for Mascar Table III workloads."""

from __future__ import annotations

import argparse
import csv
import os
from collections import Counter
from pathlib import Path


REPO_ROOT = Path(__file__).resolve().parents[4]
WORKLOAD_DIR = REPO_ROOT / "experiments" / "paper-mascar" / "workloads"
CANONICAL = WORKLOAD_DIR / "mascar_table_iii_workload_manifest.csv"
AUDITED = WORKLOAD_DIR / "mascar_table_iii_workload_manifest_audited.csv"
SUMMARY = WORKLOAD_DIR / "mascar_workload_availability_summary.csv"
AUDIT_DIR = WORKLOAD_DIR / "audit"
CANDIDATES = AUDIT_DIR / "workload_candidate_paths.txt"
SUMMARY_MD = AUDIT_DIR / "workload_availability_summary.md"
GPGPU_WORKLOADS_MANIFEST = Path("/workspace/repos/gpgpu-workloads/manifests/workload_manifest.csv")

SEARCH_ROOTS = [
    Path("/workspace/repos/gpgpu-workloads"),
    Path("/workspace/repos/gpgpu-sim_simulations"),
    Path("/workspace/repos/gpgpu-sim_distribution"),
    Path("/workspace/repos"),
    Path("/workspace"),
]

ALIASES = {
    "bp_1": ["backprop", "bp"],
    "bp_2": ["backprop", "bp"],
    "bfs": ["bfs"],
    "histo_1": ["histo", "histogram"],
    "histo_2": ["histo", "histogram"],
    "histo_3": ["histo", "histogram"],
    "histogram": ["histogram", "histo"],
    "kmeans_1": ["kmeans"],
    "kmeans_2": ["kmeans"],
    "lavamd": ["lavamd", "lava"],
    "lbm": ["lbm"],
    "leuko_1": ["leukocyte", "leuko"],
    "leuko_2": ["leukocyte", "leuko"],
    "leuko_3": ["leukocyte", "leuko"],
    "mri_q": ["mri-q", "mriq", "mri_q"],
    "mrig_1": ["mri-gridding", "mrig", "mri-gridding"],
    "mrig_2": ["mri-gridding", "mrig", "mri-gridding"],
    "mrig_3": ["mri-gridding", "mrig", "mri-gridding"],
    "mummer": ["mummer", "mummergpu"],
    "particle": ["particlefilter", "particle"],
    "pathfinder": ["pathfinder"],
    "sad_1": ["sad"],
    "sad_2": ["sad"],
    "sgemm": ["sgemm"],
    "spmv": ["spmv"],
    "srad_1": ["srad"],
    "srad_2": ["srad"],
    "stencil": ["stencil"],
    "tpacf": ["tpacf"],
    "cutcp": ["cutcp"],
}

SKIP_DIRS = {
    ".git",
    ".hg",
    ".cache",
    "__pycache__",
    "build",
    "build_logs",
    "debug",
    "debug_tools",
    "lib",
    "logs",
    "m5_runs",
    "node_modules",
    "obj",
    "review_packs",
    "runs",
    "tmp",
}

SOURCE_GUESSES = {
    "lbm": "/workspace/repos/gpgpu-workloads/suites/parboil/benchmarks/lbm",
    "leuko_1": "/workspace/repos/gpgpu-workloads/suites/rodinia/cuda/leukocyte",
    "leuko_2": "/workspace/repos/gpgpu-workloads/suites/rodinia/cuda/leukocyte",
    "leuko_3": "/workspace/repos/gpgpu-workloads/suites/rodinia/cuda/leukocyte",
    "mrig_1": "/workspace/repos/gpgpu-workloads/suites/parboil/benchmarks/mri-gridding",
    "mrig_2": "/workspace/repos/gpgpu-workloads/suites/parboil/benchmarks/mri-gridding",
    "mrig_3": "/workspace/repos/gpgpu-workloads/suites/parboil/benchmarks/mri-gridding",
    "mummer": "/workspace/repos/gpgpu-workloads/suites/rodinia/cuda/mummergpu",
    "particle": "/workspace/repos/gpgpu-workloads/suites/rodinia/cuda/particlefilter",
    "sad_1": "/workspace/repos/gpgpu-workloads/suites/parboil/benchmarks/sad",
    "sad_2": "/workspace/repos/gpgpu-workloads/suites/parboil/benchmarks/sad",
    "cutcp": "/workspace/repos/gpgpu-workloads/suites/parboil/benchmarks/cutcp",
    "lavamd": "/workspace/repos/gpgpu-workloads/suites/rodinia/cuda/lavaMD",
    "tpacf": "/workspace/repos/gpgpu-workloads/suites/parboil/benchmarks/tpacf",
}


def load_csv(path: Path) -> list[dict[str, str]]:
    with path.open(newline="") as f:
        return list(csv.DictReader(f))


def write_csv(path: Path, rows: list[dict[str, str]], fields: list[str]) -> None:
    path.parent.mkdir(parents=True, exist_ok=True)
    with path.open("w", newline="") as f:
        writer = csv.DictWriter(f, fieldnames=fields, lineterminator="\n")
        writer.writeheader()
        writer.writerows(rows)


def load_ready_workloads() -> dict[str, dict[str, str]]:
    if not GPGPU_WORKLOADS_MANIFEST.exists():
        return {}
    rows = load_csv(GPGPU_WORKLOADS_MANIFEST)
    return {row["app"]: row for row in rows if row.get("status") == "ready"}


def path_exists(value: str) -> bool:
    return bool(value) and Path(value).exists()


def search_candidates(aliases: list[str], max_hits: int = 40) -> list[str]:
    aliases_lower = [a.lower() for a in aliases]
    hits: list[str] = []
    seen: set[str] = set()
    for root in SEARCH_ROOTS:
        if not root.exists():
            continue
        for cur_root, dirnames, filenames in os.walk(root):
            dirnames[:] = [d for d in dirnames if d not in SKIP_DIRS and not d.startswith(".")]
            current = Path(cur_root)
            names = [current.name, *filenames]
            if any(alias in name.lower() for alias in aliases_lower for name in names):
                candidate = str(current)
                if candidate not in seen:
                    hits.append(candidate)
                    seen.add(candidate)
                    if len(hits) >= max_hits:
                        return hits
    return hits


def recommended_action(row: dict[str, str]) -> str:
    status = row["availability_status"]
    phase = row["phase_mapping_status"]
    if status == "available" and row["run_command_status"] == "ready":
        return "ready_for_w3_dryrun_and_short_smoke"
    if status == "partial_phase_unknown" or phase == "app_only":
        return "resolve_paper_phase_to_local_kernel_before_actual_table_iii_run"
    if status == "available_needs_build":
        return "add_bounded_build_command_and_verify_binary"
    if status == "missing_binary":
        return "build_or_import_binary"
    if status == "missing_data":
        return "locate_or_import_input_data"
    if status == "missing_source":
        return "import_or_point_to_benchmark_source"
    return "manual_audit_required"


def audit_rows(rows: list[dict[str, str]]) -> tuple[list[dict[str, str]], dict[str, list[str]]]:
    ready = load_ready_workloads()
    candidate_map: dict[str, list[str]] = {}
    audited: list[dict[str, str]] = []
    for row in rows:
        row = dict(row)
        paper_id = row["paper_id"]
        candidate_map[paper_id] = search_candidates(ALIASES.get(paper_id, [row["paper_app"]]))

        app_guess = row.get("local_app_guess", "")
        ready_row = ready.get(app_guess)
        if ready_row:
            row["local_path"] = row["local_path"] or ready_row.get("config_local", "")
            row["local_binary"] = row["local_binary"] or ready_row.get("binary_path", "")
            row["local_command"] = row["local_command"] or f"bash scripts/run_one.sh {app_guess}"
            row["build_status"] = "ready"
            if row["phase_mapping_status"] == "app_only":
                row["availability_status"] = "partial_phase_unknown"
                row["run_command_status"] = "placeholder_phase_unknown"
            elif row["run_command_status"] == "ready":
                row["availability_status"] = "available"
        elif path_exists(row.get("local_path", "")) and not path_exists(row.get("local_binary", "")):
            row["availability_status"] = "missing_binary"
            row["run_command_status"] = "placeholder_missing_binary"
        elif paper_id in SOURCE_GUESSES and Path(SOURCE_GUESSES[paper_id]).exists():
            row["local_path"] = SOURCE_GUESSES[paper_id]
            row["availability_status"] = "missing_binary"
            row["run_command_status"] = "placeholder_missing_binary"

        audited.append(row)
    return audited, candidate_map


def write_candidate_paths(candidate_map: dict[str, list[str]]) -> None:
    AUDIT_DIR.mkdir(parents=True, exist_ok=True)
    with CANDIDATES.open("w") as f:
        for paper_id, hits in candidate_map.items():
            f.write(f"## {paper_id}\n")
            if not hits:
                f.write("no_candidates_found\n")
            else:
                for hit in hits:
                    f.write(f"{hit}\n")
            f.write("\n")


def write_summary_files(rows: list[dict[str, str]]) -> None:
    summary_fields = [
        "paper_id",
        "paper_name",
        "paper_type",
        "availability_status",
        "phase_mapping_status",
        "local_path",
        "local_binary",
        "local_data_path",
        "recommended_action",
    ]
    summary_rows = []
    for row in rows:
        summary_rows.append(
            {
                "paper_id": row["paper_id"],
                "paper_name": row["paper_name"],
                "paper_type": row["paper_type"],
                "availability_status": row["availability_status"],
                "phase_mapping_status": row["phase_mapping_status"],
                "local_path": row["local_path"],
                "local_binary": row["local_binary"],
                "local_data_path": row["local_data_path"],
                "recommended_action": recommended_action(row),
            }
        )
    write_csv(SUMMARY, summary_rows, summary_fields)

    availability = Counter(row["availability_status"] for row in rows)
    phases = Counter(row["phase_mapping_status"] for row in rows)
    ready = [row for row in rows if row["run_command_status"] == "ready"]
    placeholders = [row for row in rows if row["run_command_status"] != "ready"]
    with SUMMARY_MD.open("w") as f:
        f.write("# Mascar Table III Workload Availability Summary\n\n")
        f.write(f"Rows audited: {len(rows)}\n\n")
        f.write("## Availability Counts\n\n")
        for key, value in sorted(availability.items()):
            f.write(f"- {key}: {value}\n")
        f.write("\n## Phase Mapping Counts\n\n")
        for key, value in sorted(phases.items()):
            f.write(f"- {key}: {value}\n")
        f.write("\n## Ready Wrapper Candidates\n\n")
        for row in ready:
            f.write(f"- {row['paper_name']} -> {row['local_app_guess']}\n")
        f.write("\n## Placeholder Rows\n\n")
        for row in placeholders:
            f.write(
                f"- {row['paper_name']}: {row['availability_status']} "
                f"phase={row['phase_mapping_status']} action={recommended_action(row)}\n"
            )


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--manifest", type=Path, default=CANONICAL)
    parser.add_argument("--audited", type=Path, default=AUDITED)
    args = parser.parse_args()

    rows = load_csv(args.manifest)
    if len(rows) != 30:
        raise SystemExit(f"expected 30 Table III rows, found {len(rows)} in {args.manifest}")
    audited, candidate_map = audit_rows(rows)
    write_csv(args.audited, audited, list(rows[0].keys()))
    write_candidate_paths(candidate_map)
    write_summary_files(audited)
    print(f"Audited {len(audited)} Table III rows")
    print(f"Wrote {args.audited}")
    print(f"Wrote {SUMMARY}")
    print(f"Wrote {CANDIDATES}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
