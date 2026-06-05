#!/usr/bin/env bash
set -euo pipefail
script_dir="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
repo_root="$(cd "${script_dir}/../../../../.." && pwd)"
common_runner="${repo_root}/experiments/common/gpgpusim_matrix/run_gpgpusim_matrix.sh"
W24_CONFIG_MATRIX="${W24_CONFIG_MATRIX:-${script_dir}/w24_tableiii_config_matrix.csv}"
W24_WORKLOAD_MANIFEST="${W24_WORKLOAD_MANIFEST:-${script_dir}/w24_tableiii_runner_workload_manifest.csv}"
W24_MODE="${W24_MODE:-ready}"
W24_TIMEOUT_SEC="${W24_TIMEOUT_SEC:-1800}"
W24_MAX_RUNS="${W24_MAX_RUNS:-0}"
W24_DRY_RUN_ONLY="${W24_DRY_RUN_ONLY:-0}"
W24_FILTER_PAPER_ID="${W24_FILTER_PAPER_ID:-}"
W24_FILTER_CONFIG_ID="${W24_FILTER_CONFIG_ID:-}"
W24_OUTDIR="${W24_OUTDIR:-${repo_root}/experiments/paper-mascar/workloads/results/W24/w24_sweep_${W24_MODE}_$(date '+%Y%m%d_%H%M%S')}"
mkdir -p "${W24_OUTDIR}"
cp -f "${W24_CONFIG_MATRIX}" "${W24_OUTDIR}/w24_config_matrix.snapshot.csv"
cp -f "${W24_WORKLOAD_MANIFEST}" "${W24_OUTDIR}/w24_workload_manifest.snapshot.csv"
export CONFIG_MATRIX="${W24_CONFIG_MATRIX}"
export WORKLOAD_MANIFEST="${W24_WORKLOAD_MANIFEST}"
export OUTDIR="${W24_OUTDIR}"
export TIMEOUT_SEC="${W24_TIMEOUT_SEC}"
export MAX_RUNS="${W24_MAX_RUNS}"
export RUN_PLACEHOLDERS=0
export RUN_READY=1
export DRY_RUN_ONLY="${W24_DRY_RUN_ONLY}"
export SOURCE_ENV=1
export USE_MANIFEST_TIMEOUT=0
export ONLY_READY=1
export FILTER_PAPER_ID="${W24_FILTER_PAPER_ID}"
export FILTER_CONFIG_ID="${W24_FILTER_CONFIG_ID}"
bash "${common_runner}"
printf 'w24_outdir=%s\n' "${W24_OUTDIR}"
