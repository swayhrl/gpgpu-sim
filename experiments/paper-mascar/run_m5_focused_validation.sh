#!/usr/bin/env bash
set -uo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
REPO_ROOT="$(cd "${SCRIPT_DIR}/../.." && pwd)"

M5_CONFIG_MATRIX="${M5_CONFIG_MATRIX:-${SCRIPT_DIR}/m5_config_matrix.csv}"
M5_WORKLOAD_MANIFEST="${M5_WORKLOAD_MANIFEST:-${SCRIPT_DIR}/m5_workload_manifest.csv}"
M5_OUTDIR="${M5_OUTDIR:-${SCRIPT_DIR}/m5_runs/$(date '+%Y%m%d_%H%M%S')}"
M5_TIMEOUT_SEC="${M5_TIMEOUT_SEC:-1200}"
M5_MAX_RUNS="${M5_MAX_RUNS:-0}"

mkdir -p "${M5_OUTDIR}"
RUN_MANIFEST="${M5_OUTDIR}/run_manifest.csv"
printf 'config_id,workload_id,exit_code,log_path,run_dir,config_path,command\n' > "${RUN_MANIFEST}"

if [[ -f "${REPO_ROOT}/setup_environment" ]]; then
  set +u
  # shellcheck disable=SC1091
  source "${REPO_ROOT}/setup_environment" release
  set -u
fi

available_count=$(awk -F',' 'NR>1 && $7=="available" {c++} END {print c+0}' "${M5_WORKLOAD_MANIFEST}")
if [[ "${available_count}" -eq 0 ]]; then
  echo "No available workloads in ${M5_WORKLOAD_MANIFEST}."
  echo "Edit the manifest or provide external benchmark binaries, then rerun."
  exit 0
fi

run_count=0
tail -n +2 "${M5_CONFIG_MATRIX}" | while IFS=',' read -r config_id config_path purpose enable l1sat m2 m3 m4 old_proxy expected; do
  [[ -z "${config_id}" ]] && continue
  abs_config="${REPO_ROOT}/${config_path}"
  if [[ ! -d "${abs_config}" ]]; then
    echo "[WARN] missing config ${abs_config}; skipping ${config_id}"
    continue
  fi

  tail -n +2 "${M5_WORKLOAD_MANIFEST}" | while IFS=',' read -r workload_id suite type command working_dir expected_runtime status notes; do
    [[ "${status}" == "available" ]] || continue
    if [[ "${M5_MAX_RUNS}" -gt 0 && "${run_count}" -ge "${M5_MAX_RUNS}" ]]; then
      continue
    fi
    run_count=$((run_count + 1))
    safe_name="${config_id}__${workload_id}"
    per_run="${M5_OUTDIR}/${safe_name}"
    mkdir -p "${per_run}"
    cp -f "${abs_config}"/*.config "${per_run}/" 2>/dev/null || true
    cp -f "${abs_config}"/*.icnt "${per_run}/" 2>/dev/null || true
    log_path="${per_run}/run.log"

    echo "[RUN] config=${config_id} workload=${workload_id}"
    echo "      workdir=${working_dir}"
    echo "      log=${log_path}"
    before_dir=$(find /workspace/repos/gpgpu-workloads/runs/"${workload_id}" -mindepth 1 -maxdepth 1 -type d 2>/dev/null | sort | tail -1 || true)
    (
      cd "${working_dir}" || exit 97
      TIMEOUT_SECONDS="${M5_TIMEOUT_SEC}" \
      GPGPUSIM_CONFIG_OVERRIDE="${abs_config}" \
      timeout "${M5_TIMEOUT_SEC}" bash -lc "${command}"
    ) > "${log_path}" 2>&1
    exit_code=$?
    after_dir=$(find /workspace/repos/gpgpu-workloads/runs/"${workload_id}" -mindepth 1 -maxdepth 1 -type d 2>/dev/null | sort | tail -1 || true)
    if [[ "${after_dir}" == "${before_dir}" ]]; then
      after_dir=""
    fi
    rg -n "gpu_tot_sim_cycle|gpu_tot_ipc|paper_mascar_l1_sat_|paper_mascar_m2_|paper_mascar_m3_|paper_mascar_m4_|cacheinst_L1D|L1D" "${log_path}" > "${per_run}/stats.txt" 2>/dev/null || true
    printf '%s,%s,%s,%s,%s,%s,%s\n' "${config_id}" "${workload_id}" "${exit_code}" "${log_path}" "${after_dir}" "${config_path}" "${command}" >> "${RUN_MANIFEST}"
    if [[ "${exit_code}" -ne 0 ]]; then
      echo "[WARN] run failed: config=${config_id} workload=${workload_id} exit=${exit_code}"
      tail -40 "${log_path}" || true
    fi
  done
done

echo "M5 focused validation outdir: ${M5_OUTDIR}"
echo "Run manifest: ${RUN_MANIFEST}"
