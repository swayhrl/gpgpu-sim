#!/usr/bin/env bash
set -euo pipefail
repo_root="$(cd "$(dirname "${BASH_SOURCE[0]}")/../../../../.." && pwd)"
outdir="${repo_root}/experiments/paper-mascar/workloads/results/W15C/w15c_micro_actual"
wrapper="${repo_root}/experiments/paper-mascar/workloads/micro/mascar_m3_diag/run_m3_diag_wrapper.sh"
mkdir -p "${outdir}/runs"
printf 'strategy_id,config_id,run_dir,timeout_sec,exit_code,timeout,start_iso,end_iso,elapsed_sec,command\n' > "${outdir}/run_manifest.csv"
run_one() {
  local sid="$1" config_id="$2" config_dir="$3"
  local run_dir="${outdir}/runs/strategy_${sid}_${config_id}"
  mkdir -p "${run_dir}"
  local start_iso end_iso start_sec end_sec ec timeout_flag
  start_iso="$(date -Is)"; start_sec="$(date +%s)"
  set +e
  MASCAR_RUN_DIR="${run_dir}" MASCAR_CONFIG_DIR="${repo_root}/${config_dir}" MASCAR_M3_DIAG_STRATEGY="${sid}" timeout 120 "${wrapper}" > "${run_dir}/stdout.log" 2> "${run_dir}/stderr.log"
  ec=$?
  set -e
  end_iso="$(date -Is)"; end_sec="$(date +%s)"
  cat "${run_dir}/stdout.log" "${run_dir}/stderr.log" > "${run_dir}/combined.log"
  timeout_flag=0; [[ "${ec}" == 124 || "${ec}" == 137 ]] && timeout_flag=1
  printf '"%s","%s","%s","120","%s","%s","%s","%s","%s","MASCAR_M3_DIAG_STRATEGY=%s %s"\n' "${sid}" "${config_id}" "${run_dir}" "${ec}" "${timeout_flag}" "${start_iso}" "${end_iso}" "$((end_sec-start_sec))" "${sid}" "${wrapper}" >> "${outdir}/run_manifest.csv"
}
run_one 1 m3diag_on configs/hrl-repro/SM7_QV100_mascar_m3diag_on
run_one 2 m3diag_forced_mp_on configs/hrl-repro/SM7_QV100_mascar_m3diag_forced_mp_on
run_one 3 m3diag_forced_mp_on configs/hrl-repro/SM7_QV100_mascar_m3diag_forced_mp_on
run_one 4 m3diag_forced_mp_on configs/hrl-repro/SM7_QV100_mascar_m3diag_forced_mp_on
run_one 5 m3diag_forced_mp_on configs/hrl-repro/SM7_QV100_mascar_m3diag_forced_mp_on
printf 'outdir=%s\n' "${outdir}"
