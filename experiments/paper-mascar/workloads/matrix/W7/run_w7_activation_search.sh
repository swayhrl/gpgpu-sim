#!/usr/bin/env bash
set -euo pipefail

script_dir="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
repo_root="$(cd "${script_dir}/../../../../.." && pwd)"

W7_ITERATION="${W7_ITERATION:-iter1}"
W7_TIMEOUT_SEC="${W7_TIMEOUT_SEC:-420}"
DRY_RUN_ONLY="${DRY_RUN_ONLY:-0}"
W7_MAX_RUNS="${W7_MAX_RUNS:-0}"

case "${W7_ITERATION}" in
  iter1)
    workload_manifest="${script_dir}/w7_iter1_command_manifest.csv"
    ;;
  *)
    echo "unknown W7_ITERATION=${W7_ITERATION}" >&2
    exit 2
    ;;
esac

if [[ "${DRY_RUN_ONLY}" == "1" ]]; then
  default_outdir="${script_dir}/results/${W7_ITERATION}_dryrun_$(date '+%Y%m%d_%H%M%S')"
else
  default_outdir="${script_dir}/results/${W7_ITERATION}_actual_$(date '+%Y%m%d_%H%M%S')"
fi

export CONFIG_MATRIX="${CONFIG_MATRIX:-${script_dir}/w7_config_matrix.csv}"
export WORKLOAD_MANIFEST="${WORKLOAD_MANIFEST:-${workload_manifest}}"
export OUTDIR="${OUTDIR:-${default_outdir}}"
export TIMEOUT_SEC="${TIMEOUT_SEC:-${W7_TIMEOUT_SEC}}"
export MAX_RUNS="${MAX_RUNS:-${W7_MAX_RUNS}}"
export RUN_PLACEHOLDERS=0
export RUN_READY=1
export DRY_RUN_ONLY
export SOURCE_ENV="${SOURCE_ENV:-1}"
export GPGPUSIM_ROOT="${GPGPUSIM_ROOT:-${repo_root}}"
export GPGPU_WORKLOAD_ROOT="${GPGPU_WORKLOAD_ROOT:-/workspace/repos/gpgpu-workloads}"

bash "${repo_root}/experiments/common/gpgpusim_matrix/run_gpgpusim_matrix.sh"
