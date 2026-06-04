#!/usr/bin/env bash
set -euo pipefail

script_dir="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
repo_root="$(cd "${script_dir}/../../../../.." && pwd)"
results_root="${repo_root}/experiments/paper-mascar/workloads/results/W8"

W8_TIMEOUT_SEC="${W8_TIMEOUT_SEC:-600}"
W8_MAX_RUNS="${W8_MAX_RUNS:-0}"
DRY_RUN_ONLY="${DRY_RUN_ONLY:-0}"

workload_manifest="${script_dir}/w8_command_manifest_subset.csv"
if [[ ! -f "${workload_manifest}" ]]; then
  echo "missing workload manifest: ${workload_manifest}" >&2
  exit 2
fi

if [[ "${DRY_RUN_ONLY}" == "1" ]]; then
  default_outdir="${results_root}/w8_focused_dryrun_$(date '+%Y%m%d_%H%M%S')"
else
  default_outdir="${results_root}/w8_focused_actual_$(date '+%Y%m%d_%H%M%S')"
fi

export CONFIG_MATRIX="${CONFIG_MATRIX:-${script_dir}/w8_config_matrix.csv}"
export WORKLOAD_MANIFEST="${WORKLOAD_MANIFEST:-${workload_manifest}}"
export OUTDIR="${OUTDIR:-${default_outdir}}"
export TIMEOUT_SEC="${TIMEOUT_SEC:-${W8_TIMEOUT_SEC}}"
export MAX_RUNS="${MAX_RUNS:-${W8_MAX_RUNS}}"
export RUN_PLACEHOLDERS=0
export RUN_READY=1
export DRY_RUN_ONLY
export SOURCE_ENV="${SOURCE_ENV:-1}"
export GPGPUSIM_ROOT="${GPGPUSIM_ROOT:-${repo_root}}"
export GPGPU_WORKLOAD_ROOT="${GPGPU_WORKLOAD_ROOT:-/workspace/repos/gpgpu-workloads}"

bash "${repo_root}/experiments/common/gpgpusim_matrix/run_gpgpusim_matrix.sh"
