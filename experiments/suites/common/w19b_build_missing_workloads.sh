#!/usr/bin/env bash
set -euo pipefail

repo_root="$(cd "$(dirname "${BASH_SOURCE[0]}")/../../.." && pwd)"
out_dir="${repo_root}/experiments/suites/common"
rodinia_manifest="${out_dir}/rodinia_full_manifest.csv"
parboil_manifest="${out_dir}/parboil_full_manifest.csv"
workload_root="/workspace/repos/gpgpu-workloads"
log_root="${W19_BUILD_LOG_ROOT:-/workspace/tmp/mascar_w19_build_logs_$(date '+%Y%m%d_%H%M%S')}"
timeout_sec="${W19_BUILD_TIMEOUT_SEC:-180}"
mkdir -p "${log_root}" "${out_dir}"

build_csv="${out_dir}/build_results.csv"
binary_csv="${out_dir}/binary_recovery_results.csv"
data_csv="${out_dir}/data_availability_results.csv"
printf 'suite,suite_workload_id,benchmark,round,method,work_dir,command,exit_code,timeout,log_path,result_status,notes\n' > "${build_csv}"
printf 'suite,suite_workload_id,benchmark,binary_path,binary_exists,binary_executable,recovery_status,notes\n' > "${binary_csv}"
printf 'suite,suite_workload_id,benchmark,data_path,data_exists,data_file_count,data_status,notes\n' > "${data_csv}"

source_env() {
  if [[ -f /workspace/repos/gpgpu-sim_distribution/setup_environment ]]; then
    set +u
    # shellcheck disable=SC1091
    source /workspace/repos/gpgpu-sim_distribution/setup_environment release >/dev/null 2>&1 || true
    set -u
  fi
}

run_attempt() {
  local suite="$1" id="$2" bench="$3" round="$4" method="$5" work_dir="$6" command="$7"
  local safe_id="${id//[^A-Za-z0-9_.-]/_}"
  local log_path="${log_root}/${safe_id}_${round}_${method}.log"
  local exit_code=0 timeout_flag=0 status="attempted"
  if [[ ! -d "${work_dir}" ]]; then
    exit_code=127; status="missing_work_dir"
    printf 'missing work dir: %s\n' "${work_dir}" > "${log_path}"
  else
    ( cd "${work_dir}" && source_env && timeout "${timeout_sec}" bash -lc "${command}" ) > "${log_path}" 2>&1 || exit_code=$?
    if [[ "${exit_code}" == "124" ]]; then timeout_flag=1; status="timeout"; elif [[ "${exit_code}" == "0" ]]; then status="pass"; else status="fail"; fi
  fi
  printf '%s,%s,%s,%s,%s,%s,%s,%s,%s,%s,%s,%s\n' \
    "${suite}" "${id}" "${bench}" "${round}" "${method}" "${work_dir}" "${command//,/; }" "${exit_code}" "${timeout_flag}" "${log_path}" "${status}" "W19B build/recovery attempt" >> "${build_csv}"
}

# Round 1: existing wrapper Makefiles, if any. This confirms current ready wrappers and catches simple rebuilds.
while IFS=',' read -r suite id bench source_dir source_exists cuda_count makefile wrapper_dir wrapper_exists binary_path binary_exists binary_exec data_path data_exists data_count manifest_app manifest_status run_command availability blocker next_action notes; do
  [[ "${suite}" == "suite" ]] && continue
  if [[ "${wrapper_exists}" == "1" && -f "${wrapper_dir}/Makefile" ]]; then
    run_attempt "${suite}" "${id}" "${bench}" "round1" "wrapper_make" "${wrapper_dir}" "make"
  fi
done < <(cat "${rodinia_manifest}" "${parboil_manifest}" | awk 'NR==1 || $1!="suite"')

# Round 2: raw source build for non-ready rows.
while IFS=',' read -r suite id bench source_dir source_exists cuda_count makefile wrapper_dir wrapper_exists binary_path binary_exists binary_exec data_path data_exists data_count manifest_app manifest_status run_command availability blocker next_action notes; do
  [[ "${suite}" == "suite" ]] && continue
  [[ "${availability}" == "ready" ]] && continue
  [[ "${source_exists}" != "1" ]] && continue
  if [[ "${suite}" == "rodinia" ]]; then
    if [[ -n "${makefile}" ]]; then
      run_attempt "${suite}" "${id}" "${bench}" "round2" "raw_make" "${source_dir}" "make"
    fi
    if [[ -f "${source_dir}/Makefile_nvidia" ]]; then
      run_attempt "${suite}" "${id}" "${bench}" "round2" "raw_makefile_nvidia" "${source_dir}" "make -f Makefile_nvidia"
    elif [[ -f "${source_dir}/makefile" ]]; then
      run_attempt "${suite}" "${id}" "${bench}" "round2" "raw_lowercase_makefile" "${source_dir}" "make -f makefile"
    fi
  elif [[ "${suite}" == "parboil" ]]; then
    parboil_root="${workload_root}/suites/parboil"
    if [[ -d "${source_dir}/src/cuda" || -d "${source_dir}/src/cuda_base" || -d "${source_dir}/src/cuda-base" ]]; then
      run_attempt "${suite}" "${id}" "${bench}" "round2" "parboil_compile_cuda" "${parboil_root}" "./parboil compile ${bench} cuda default"
      run_attempt "${suite}" "${id}" "${bench}" "round2" "parboil_compile_cuda_base" "${parboil_root}" "./parboil compile ${bench} cuda_base default"
    fi
  fi
done < <(cat "${rodinia_manifest}" "${parboil_manifest}" | awk 'NR==1 || $1!="suite"')

# Rescan binary/data availability from W19A manifests after build attempts.
python3 "${out_dir}/w19a_inventory_audit.py" >/dev/null
python3 - <<'PY'
import csv, os
from pathlib import Path
out=Path('experiments/suites/common')
rows=[]
for f in [out/'rodinia_full_manifest.csv', out/'parboil_full_manifest.csv']:
    rows += list(csv.DictReader(f.open()))
with (out/'binary_recovery_results.csv').open('a', newline='') as bf:
    bw=csv.writer(bf, lineterminator='\n')
    for r in rows:
        status = 'binary_ready' if r['binary_executable']=='1' else 'missing_binary'
        if r['availability_status']=='ready': status='ready_existing_manifest'
        bw.writerow([r['suite'],r['suite_workload_id'],r['benchmark'],r['binary_path'],r['binary_exists'],r['binary_executable'],status,r['blocker']])
with (out/'data_availability_results.csv').open('a', newline='') as df:
    dw=csv.writer(df, lineterminator='\n')
    for r in rows:
        status='data_available' if r['data_exists']=='1' else 'missing_or_unverified_data'
        dw.writerow([r['suite'],r['suite_workload_id'],r['benchmark'],r['data_path'],r['data_exists'],r['data_file_count'],status,r['next_action']])
PY
printf 'w19_build_log_root=%s\n' "${log_root}"
