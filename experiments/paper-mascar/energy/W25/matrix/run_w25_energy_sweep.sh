#!/usr/bin/env bash
set -euo pipefail
script_dir="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
repo_root="$(cd "${script_dir}/../../../../.." && pwd)"
common_runner="${repo_root}/experiments/common/gpgpusim_matrix/run_gpgpusim_matrix.sh"
W25_CONFIG_MATRIX="${W25_CONFIG_MATRIX:-${script_dir}/w25_energy_config_matrix.csv}"
W25_WORKLOAD_MANIFEST="${W25_WORKLOAD_MANIFEST:-${script_dir}/w25_energy_command_manifest.csv}"
W25_TIMEOUT_SEC="${W25_TIMEOUT_SEC:-1800}"
W25_MAX_RUNS="${W25_MAX_RUNS:-0}"
W25_DRY_RUN_ONLY="${W25_DRY_RUN_ONLY:-0}"
W25_FILTER_WORKLOAD="${W25_FILTER_WORKLOAD:-}"
W25_FILTER_CONFIG="${W25_FILTER_CONFIG:-}"
W25_OUTDIR="${W25_OUTDIR:-${repo_root}/experiments/paper-mascar/energy/W25/results/w25_energy_$(date '+%Y%m%d_%H%M%S')}"
mkdir -p "${W25_OUTDIR}"
cp -f "${W25_CONFIG_MATRIX}" "${W25_OUTDIR}/w25_config_matrix.snapshot.csv"
cp -f "${W25_WORKLOAD_MANIFEST}" "${W25_OUTDIR}/w25_workload_manifest.snapshot.csv"
export CONFIG_MATRIX="${W25_CONFIG_MATRIX}"
export WORKLOAD_MANIFEST="${W25_WORKLOAD_MANIFEST}"
export OUTDIR="${W25_OUTDIR}"
export TIMEOUT_SEC="${W25_TIMEOUT_SEC}"
export MAX_RUNS="${W25_MAX_RUNS}"
export RUN_PLACEHOLDERS=0
export RUN_READY=1
export DRY_RUN_ONLY="${W25_DRY_RUN_ONLY}"
export SOURCE_ENV=1
export USE_MANIFEST_TIMEOUT=0
export ONLY_READY=1
export FILTER_PAPER_ID="${W25_FILTER_WORKLOAD}"
export FILTER_CONFIG_ID="${W25_FILTER_CONFIG}"
bash "${common_runner}"
printf 'w25_outdir=%s\n' "${W25_OUTDIR}"
