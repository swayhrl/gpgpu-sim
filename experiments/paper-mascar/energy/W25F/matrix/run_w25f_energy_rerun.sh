#!/usr/bin/env bash
set -euo pipefail
script_dir="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
repo_root="$(cd "${script_dir}/../../../../.." && pwd)"
common_runner="${repo_root}/experiments/common/gpgpusim_matrix/run_gpgpusim_matrix.sh"
W25F_CONFIG_MATRIX="${W25F_CONFIG_MATRIX:-${script_dir}/w25f_energy_config_matrix.csv}"
W25F_WORKLOAD_MANIFEST="${W25F_WORKLOAD_MANIFEST:-${script_dir}/w25f_energy_command_manifest.csv}"
W25F_TIMEOUT_SEC="${W25F_TIMEOUT_SEC:-1800}"
W25F_MAX_RUNS="${W25F_MAX_RUNS:-0}"
W25F_DRY_RUN_ONLY="${W25F_DRY_RUN_ONLY:-0}"
W25F_FILTER_WORKLOAD="${W25F_FILTER_WORKLOAD:-}"
W25F_FILTER_CONFIG="${W25F_FILTER_CONFIG:-}"
W25F_OUTDIR="${W25F_OUTDIR:-${repo_root}/experiments/paper-mascar/energy/W25F/results/w25f_energy_$(date '+%Y%m%d_%H%M%S')}"
mkdir -p "${W25F_OUTDIR}"
cp -f "${W25F_CONFIG_MATRIX}" "${W25F_OUTDIR}/w25f_config_matrix.snapshot.csv"
cp -f "${W25F_WORKLOAD_MANIFEST}" "${W25F_OUTDIR}/w25f_workload_manifest.snapshot.csv"
CONFIG_MATRIX="${W25F_CONFIG_MATRIX}" \
WORKLOAD_MANIFEST="${W25F_WORKLOAD_MANIFEST}" \
COMMAND_MANIFEST="${W25F_WORKLOAD_MANIFEST}" \
OUTDIR="${W25F_OUTDIR}" \
TIMEOUT_SEC="${W25F_TIMEOUT_SEC}" \
MAX_RUNS="${W25F_MAX_RUNS}" \
RUN_PLACEHOLDERS=0 \
RUN_READY=1 \
DRY_RUN_ONLY="${W25F_DRY_RUN_ONLY}" \
SOURCE_ENV=1 \
USE_MANIFEST_TIMEOUT=0 \
ONLY_READY=1 \
FILTER_PAPER_ID="${W25F_FILTER_WORKLOAD}" \
FILTER_CONFIG_ID="${W25F_FILTER_CONFIG}" \
bash "${common_runner}"
printf 'w25f_outdir=%s\n' "${W25F_OUTDIR}"
