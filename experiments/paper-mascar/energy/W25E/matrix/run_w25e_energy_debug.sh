#!/usr/bin/env bash
set -euo pipefail
script_dir="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
repo_root="$(cd "${script_dir}/../../../../.." && pwd)"
config_matrix="${W25E_CONFIG_MATRIX:-${script_dir}/w25e_energy_debug_config_matrix.csv}"
workload_manifest="${W25E_WORKLOAD_MANIFEST:-${script_dir}/w25e_energy_debug_workload_manifest.csv}"
outdir="${W25E_OUTDIR:-${repo_root}/experiments/paper-mascar/energy/W25E/results/w25e_energy_debug_$(date '+%Y%m%d_%H%M%S')}"
timeout_sec="${W25E_TIMEOUT_SEC:-1800}"
strategy="${W25E_STRATEGY:-artifact_recovery}"
workload_root="${GPGPU_WORKLOAD_ROOT:-/workspace/repos/gpgpu-workloads}"
mkdir -p "${outdir}/runs"
cp -f "${config_matrix}" "${outdir}/w25e_config_matrix.snapshot.csv"
cp -f "${workload_manifest}" "${outdir}/w25e_workload_manifest.snapshot.csv"
manifest_out="${outdir}/run_manifest.csv"
printf 'run_id,config_id,config_path,paper_id,paper_name,paper_type,wrapper_path,wrapper_status,run_dir,timeout_sec,exit_code,timeout,result_status_prelim,start_iso,end_iso,elapsed_sec,notes\n' > "${manifest_out}"
if [[ -f "${repo_root}/setup_environment" ]]; then
  set +u
  # shellcheck disable=SC1091
  source "${repo_root}/setup_environment" release
  set -u
fi
abs_path() { if [[ "$1" = /* ]]; then printf '%s\n' "$1"; else printf '%s/%s\n' "${repo_root}" "$1"; fi; }
csv_quote() { local value="${1//$'\n'/ }"; value="${value//\"/\"\"}"; printf '"%s"' "${value}"; }
record_row() {
  local run_id="$1" config_id="$2" config_path="$3" paper_id="$4" paper_name="$5" paper_type="$6" wrapper_path="$7" wrapper_status="$8" run_dir="$9" row_timeout="${10}" exit_code="${11}" timeout_flag="${12}" prelim="${13}" start_iso="${14}" end_iso="${15}" elapsed="${16}" notes="${17}"
  { csv_quote "${run_id}"; printf ','; csv_quote "${config_id}"; printf ','; csv_quote "${config_path}"; printf ','; csv_quote "${paper_id}"; printf ','; csv_quote "${paper_name}"; printf ','; csv_quote "${paper_type}"; printf ','; csv_quote "${wrapper_path}"; printf ','; csv_quote "${wrapper_status}"; printf ','; csv_quote "${run_dir}"; printf ','; csv_quote "${row_timeout}"; printf ','; csv_quote "${exit_code}"; printf ','; csv_quote "${timeout_flag}"; printf ','; csv_quote "${prelim}"; printf ','; csv_quote "${start_iso}"; printf ','; csv_quote "${end_iso}"; printf ','; csv_quote "${elapsed}"; printf ','; csv_quote "${notes}"; printf '\n'; } >> "${manifest_out}"
}
copy_power_artifacts() {
  local marker="$1" run_dir="$2" run_working_dir="$3" run_command="$4"
  local dest="${run_dir}/power_artifacts"
  mkdir -p "${dest}"
  local exec_dir=""
  exec_dir="$(printf '%s\n' "${run_command}" | sed -n 's/^cd \([^ ]*\).*/\1/p')"
  local roots=("${run_working_dir}")
  [[ -n "${exec_dir}" && -d "${exec_dir}" ]] && roots+=("${exec_dir}")
  [[ -d "${workload_root}" ]] && roots+=("${workload_root}")
  local count=0
  for root_dir in "${roots[@]}"; do
    [[ -d "${root_dir}" ]] || continue
    while IFS= read -r -d '' file; do
      local base safe target
      base="$(basename "${file}")"
      safe="${root_dir//\//_}.${base}"
      target="${dest}/${safe}"
      cp -f "${file}" "${target}" 2>/dev/null || true
      count=$((count + 1))
    done < <(find "${root_dir}" -maxdepth 4 -type f -newer "${marker}" \( -iname '*power*' -o -iname '*watt*' -o -iname '*accelwattch*' -o -iname '*gpuwattch*' -o -iname '*.xml' \) -print0 2>/dev/null)
  done
  printf 'power_artifact_copy_count=%s\n' "${count}" | tee "${run_dir}/power_artifacts/status.txt"
}
while IFS=',' read -r config_id config_path config_role enabled config_notes; do
  [[ "${config_id}" == "config_id" ]] && continue
  [[ "${enabled}" == "1" ]] || continue
  config_abs="$(abs_path "${config_path}")"
  while IFS=',' read -r paper_id paper_name paper_type availability wrapper_path wrapper_status build_required build_command run_working_dir run_command input_size row_timeout dry_run_status workload_notes; do
    [[ "${paper_id}" == "paper_id" ]] && continue
    wrapper_abs="$(abs_path "${wrapper_path}")"
    run_id="${strategy}__${config_id}__${paper_id}"
    run_dir="${outdir}/runs/${strategy}/${config_id}/${paper_id}"
    mkdir -p "${run_dir}"
    marker="${run_dir}/artifact_start.marker"
    : > "${marker}"
    cp -f "${config_abs}/gpgpusim.config" "${run_dir}/gpgpusim.config" 2>/dev/null || true
    cp -f "${config_abs}/config_volta_islip.icnt" "${run_dir}/config_volta_islip.icnt" 2>/dev/null || true
    cp -f "${config_abs}"/*.xml "${run_dir}/" 2>/dev/null || true
    {
      printf 'W25E_STRATEGY=%s\n' "${strategy}"
      printf 'MASCAR_RUN_DIR=%s\n' "${run_dir}"
      printf 'MASCAR_CONFIG_DIR=%s\n' "${config_abs}"
      printf 'MASCAR_TIMEOUT_SEC=%s\n' "${row_timeout:-${timeout_sec}}"
      printf 'GPGPUSIM_ROOT=%s\n' "${repo_root}"
      printf 'GPGPU_WORKLOAD_ROOT=%s\n' "${workload_root}"
      printf 'run_working_dir=%s\n' "${run_working_dir}"
      printf 'run_command=%s\n' "${run_command}"
    } > "${run_dir}/env.txt"
    printf '%q ' "${wrapper_abs}" > "${run_dir}/command.txt"; printf '\n' >> "${run_dir}/command.txt"
    start_iso="$(date -Iseconds)"; start_sec="$(date +%s)"
    set +e
    MASCAR_RUN_DIR="${run_dir}" MASCAR_CONFIG_DIR="${config_abs}" MASCAR_TIMEOUT_SEC="${row_timeout:-${timeout_sec}}" GPGPUSIM_ROOT="${repo_root}" GPGPU_WORKLOAD_ROOT="${workload_root}" COMMAND_MANIFEST="${workload_manifest}" timeout "${row_timeout:-${timeout_sec}}" "${wrapper_abs}" > "${run_dir}/stdout.log" 2> "${run_dir}/stderr.log"
    exit_code=$?
    set -e
    if [[ "${strategy}" == "artifact_recovery" || "${strategy}" == "config_artifact_recovery" ]]; then
      copy_power_artifacts "${marker}" "${run_dir}" "${run_working_dir}" "${run_command}" >> "${run_dir}/stdout.log" 2>> "${run_dir}/stderr.log" || true
    fi
    cat "${run_dir}/stdout.log" "${run_dir}/stderr.log" > "${run_dir}/combined.log"
    end_iso="$(date -Iseconds)"; end_sec="$(date +%s)"; elapsed=$((end_sec - start_sec))
    timeout_flag=0; prelim="completed_nonzero_or_error"
    if [[ "${exit_code}" == "124" || "${exit_code}" == "137" ]]; then timeout_flag=1; prelim="timeout"; elif [[ "${exit_code}" == "77" ]]; then prelim="wrapper_unavailable"; elif [[ "${exit_code}" == "0" ]]; then prelim="completed_exit0"; fi
    record_row "${run_id}" "${config_id}" "${config_abs}" "${paper_id}" "${paper_name}" "${paper_type}" "${wrapper_path}" "${wrapper_status}" "${run_dir}" "${row_timeout:-${timeout_sec}}" "${exit_code}" "${timeout_flag}" "${prelim}" "${start_iso}" "${end_iso}" "${elapsed}" "${strategy}; ${workload_notes}; ${config_notes}"
  done < "${workload_manifest}"
done < "${config_matrix}"
python3 "${repo_root}/experiments/common/gpgpusim_matrix/collect_gpgpusim_stats.py" "${outdir}"
printf 'w25e_outdir=%s\n' "${outdir}"
