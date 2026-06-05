#!/usr/bin/env bash
set -euo pipefail
script_dir="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
repo_root="$(cd "${script_dir}/../../../.." && pwd)"
run_name="${RUN_NAME:-w16_energy_$(date '+%Y%m%d_%H%M%S')}"
outdir="${OUTDIR:-${script_dir}/results/${run_name}}"
mkdir -p "${outdir}"

CONFIG_MATRIX="${CONFIG_MATRIX:-${script_dir}/w16_energy_config_matrix.csv}" \
WORKLOAD_MANIFEST="${WORKLOAD_MANIFEST:-${script_dir}/w16_energy_workload_manifest.csv}" \
COMMAND_MANIFEST="${COMMAND_MANIFEST:-${script_dir}/w16_energy_workload_manifest.csv}" \
OUTDIR="${outdir}" \
TIMEOUT_SEC="${TIMEOUT_SEC:-1800}" \
USE_MANIFEST_TIMEOUT="${USE_MANIFEST_TIMEOUT:-1}" \
RUN_PLACEHOLDERS="0" \
RUN_READY="1" \
SOURCE_ENV="${SOURCE_ENV:-1}" \
GPGPUSIM_ROOT="${GPGPUSIM_ROOT:-${repo_root}}" \
GPGPU_WORKLOAD_ROOT="${GPGPU_WORKLOAD_ROOT:-/workspace/repos/gpgpu-workloads}" \
DRY_RUN_ONLY="${DRY_RUN_ONLY:-0}" \
bash "${repo_root}/experiments/common/gpgpusim_matrix/run_gpgpusim_matrix.sh"

python3 "${repo_root}/experiments/common/gpgpusim_matrix/collect_gpgpusim_stats.py" "${outdir}"
cp -f "${outdir}/results.csv" "${script_dir}/w16_energy_latest_results.csv"
cp -f "${outdir}/summary.md" "${script_dir}/w16_energy_latest_summary.md"
cp -f "${outdir}/status_matrix.csv" "${script_dir}/w16_energy_latest_status_matrix.csv"
cp -f "${outdir}/run_manifest.csv" "${script_dir}/w16_energy_latest_run_manifest.csv"
printf 'latest_outdir=%s\n' "${outdir}"
