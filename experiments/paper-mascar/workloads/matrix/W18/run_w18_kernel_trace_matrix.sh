#!/usr/bin/env bash
set -euo pipefail

script_dir="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
repo_root="$(cd "${script_dir}/../../../../.." && pwd)"
results_dir="${repo_root}/experiments/paper-mascar/workloads/results/W18"
config_matrix="${script_dir}/w18c_trace_config_matrix.csv"
workload_manifest="${script_dir}/w18c_trace_workload_manifest.csv"
raw_root="${W18_RAW_ROOT:-/workspace/tmp}"
timeout_default="${W18_TIMEOUT_SEC:-1800}"
stamp="$(date '+%Y%m%d_%H%M%S')"
run_outdir="${W18_RUN_OUTDIR:-${results_dir}/w18_trace_raw_${stamp}}"
mkdir -p "${run_outdir}" "${results_dir}"
manifest="${run_outdir}/run_manifest.csv"
printf 'run_id,config_id,paper_id,paper_name,paper_type,wrapper_status,wrapper_path,run_dir,exit_code,timeout,result_status_prelim\n' > "${manifest}"

if [[ -f "${repo_root}/setup_environment" ]]; then
  set +u
  # shellcheck disable=SC1091
  source "${repo_root}/setup_environment" release
  set -u
fi

while IFS=',' read -r config_id config_dir config_notes; do
  [[ "${config_id}" == "config_id" ]] && continue
  [[ -z "${config_id}" ]] && continue
  abs_config_dir="${repo_root}/${config_dir}"
  while IFS=',' read -r paper_id paper_name paper_type app wrapper_path wrapper_status timeout_sec phase_status notes; do
    [[ "${paper_id}" == "paper_id" ]] && continue
    [[ -z "${paper_id}" ]] && continue
    timeout_sec="${timeout_sec:-${timeout_default}}"
    run_id="${config_id}_${paper_id}"
    run_dir="${run_outdir}/${run_id}"
    mkdir -p "${run_dir}"
    exit_code=0
    timeout_flag=0
    result_status="completed"
    if [[ "${W18_DRY_RUN:-0}" == "1" ]]; then
      MASCAR_RUN_DIR="${run_dir}" MASCAR_CONFIG_DIR="${abs_config_dir}" MASCAR_TIMEOUT_SEC="${timeout_sec}" \
        bash "${repo_root}/${wrapper_path}" --dry-run > "${run_dir}/wrapper_driver.log" 2>&1 || exit_code=$?
      result_status="dry_run"
    else
      timeout "${timeout_sec}" env MASCAR_RUN_DIR="${run_dir}" MASCAR_CONFIG_DIR="${abs_config_dir}" MASCAR_TIMEOUT_SEC="${timeout_sec}" \
        bash "${repo_root}/${wrapper_path}" > "${run_dir}/wrapper_driver.log" 2>&1 || exit_code=$?
      if [[ "${exit_code}" == "124" ]]; then
        timeout_flag=1
        result_status="timeout"
      elif [[ "${exit_code}" == "77" ]]; then
        result_status="wrapper_unavailable"
      elif [[ "${exit_code}" != "0" ]]; then
        result_status="nonzero"
      fi
    fi
    printf '%s,%s,%s,%s,%s,%s,%s,%s,%s,%s,%s\n' \
      "${run_id}" "${config_id}" "${paper_id}" "${paper_name}" "${paper_type}" "${wrapper_status}" "${wrapper_path}" "${run_dir}" "${exit_code}" "${timeout_flag}" "${result_status}" >> "${manifest}"
  done < "${workload_manifest}"
done < "${config_matrix}"

python3 "${repo_root}/experiments/common/gpgpusim_matrix/collect_gpgpusim_stats.py" "${run_outdir}"
python3 "${repo_root}/experiments/common/gpgpusim_matrix/collect_kernel_trace.py" "${run_outdir}" --output "${run_outdir}/kernel_trace.csv"
cp -f "${manifest}" "${results_dir}/w18_latest_run_manifest.csv"
cp -f "${run_outdir}/results.csv" "${results_dir}/w18_latest_results.csv"
cp -f "${run_outdir}/summary.md" "${results_dir}/w18_latest_summary.md"
cp -f "${run_outdir}/status_matrix.csv" "${results_dir}/w18_latest_status_matrix.csv"
cp -f "${run_outdir}/kernel_trace.csv" "${results_dir}/w18_latest_kernel_trace.csv"
printf 'w18_run_outdir=%s\n' "${run_outdir}"
