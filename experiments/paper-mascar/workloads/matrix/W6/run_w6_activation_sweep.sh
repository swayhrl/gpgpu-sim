#!/usr/bin/env bash
set -euo pipefail

script_dir="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
repo_root="$(cd "${script_dir}/../../../../.." && pwd)"
results_root="${repo_root}/experiments/paper-mascar/workloads/results/W6"

W6_MODE="${W6_MODE:-ready}"
W6_TIMEOUT_SEC="${W6_TIMEOUT_SEC:-1200}"
W6_MAX_RUNS="${W6_MAX_RUNS:-0}"
DRY_RUN_ONLY="${DRY_RUN_ONLY:-0}"

case "${W6_MODE}" in
  activation_ready)
    workload_manifest="${script_dir}/w6_command_manifest_activation_ready.csv"
    ;;
  ready|all_ready)
    workload_manifest="${script_dir}/w6_command_manifest_ready.csv"
    ;;
  *)
    echo "unknown W6_MODE=${W6_MODE}" >&2
    exit 2
    ;;
esac

if [[ ! -f "${workload_manifest}" ]]; then
  echo "missing workload manifest: ${workload_manifest}" >&2
  exit 2
fi

row_count=$(( $(wc -l < "${workload_manifest}") - 1 ))
if [[ "${row_count}" -le 0 ]]; then
  outdir="${results_root}/w6_${W6_MODE}_empty_$(date '+%Y%m%d_%H%M%S')"
  mkdir -p "${outdir}"
  printf 'run_id,config_id,config_path,paper_id,paper_name,paper_type,wrapper_path,wrapper_status,run_dir,timeout_sec,exit_code,timeout,result_status_prelim,start_iso,end_iso,elapsed_sec,notes\n' > "${outdir}/run_manifest.csv"
  echo "W6_MODE=${W6_MODE} has no workload rows; wrote empty manifest ${outdir}/run_manifest.csv"
  echo "outdir=${outdir}"
  exit 0
fi

if [[ "${DRY_RUN_ONLY}" == "1" ]]; then
  default_outdir="${results_root}/w6_${W6_MODE}_dryrun_$(date '+%Y%m%d_%H%M%S')"
else
  default_outdir="${results_root}/w6_${W6_MODE}_sweep_$(date '+%Y%m%d_%H%M%S')"
fi

export CONFIG_MATRIX="${CONFIG_MATRIX:-${script_dir}/w6_config_matrix.csv}"
export WORKLOAD_MANIFEST="${WORKLOAD_MANIFEST:-${workload_manifest}}"
export OUTDIR="${OUTDIR:-${default_outdir}}"
export TIMEOUT_SEC="${TIMEOUT_SEC:-${W6_TIMEOUT_SEC}}"
export MAX_RUNS="${MAX_RUNS:-${W6_MAX_RUNS}}"
export RUN_PLACEHOLDERS=0
export RUN_READY=1
export DRY_RUN_ONLY
export SOURCE_ENV="${SOURCE_ENV:-1}"
export GPGPUSIM_ROOT="${GPGPUSIM_ROOT:-${repo_root}}"
export GPGPU_WORKLOAD_ROOT="${GPGPU_WORKLOAD_ROOT:-/workspace/repos/gpgpu-workloads}"

bash "${repo_root}/experiments/common/gpgpusim_matrix/run_gpgpusim_matrix.sh"
