#!/usr/bin/env bash
set -euo pipefail

script_dir="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
default_root="$(cd "${script_dir}/../../.." && pwd)"

CONFIG_MATRIX="${CONFIG_MATRIX:-}"
WORKLOAD_MANIFEST="${WORKLOAD_MANIFEST:-}"
OUTDIR="${OUTDIR:-${script_dir}/runs/$(date '+%Y%m%d_%H%M%S')}"
TIMEOUT_SEC="${TIMEOUT_SEC:-1200}"
MAX_RUNS="${MAX_RUNS:-0}"
RUN_PLACEHOLDERS="${RUN_PLACEHOLDERS:-1}"
RUN_READY="${RUN_READY:-1}"
DRY_RUN_ONLY="${DRY_RUN_ONLY:-0}"
SOURCE_ENV="${SOURCE_ENV:-1}"
GPGPUSIM_ROOT="${GPGPUSIM_ROOT:-${default_root}}"
GPGPU_WORKLOAD_ROOT="${GPGPU_WORKLOAD_ROOT:-/workspace/repos/gpgpu-workloads}"
USE_MANIFEST_TIMEOUT="${USE_MANIFEST_TIMEOUT:-0}"
FILTER_PAPER_ID="${FILTER_PAPER_ID:-}"
FILTER_CONFIG_ID="${FILTER_CONFIG_ID:-}"
ONLY_READY="${ONLY_READY:-0}"

if [[ -z "${CONFIG_MATRIX}" || -z "${WORKLOAD_MANIFEST}" ]]; then
  echo "CONFIG_MATRIX and WORKLOAD_MANIFEST are required" >&2
  exit 2
fi
if [[ ! -f "${CONFIG_MATRIX}" ]]; then
  echo "missing CONFIG_MATRIX: ${CONFIG_MATRIX}" >&2
  exit 2
fi
if [[ ! -f "${WORKLOAD_MANIFEST}" ]]; then
  echo "missing WORKLOAD_MANIFEST: ${WORKLOAD_MANIFEST}" >&2
  exit 2
fi

mkdir -p "${OUTDIR}/runs"
manifest_out="${OUTDIR}/run_manifest.csv"
printf 'run_id,config_id,config_path,paper_id,paper_name,paper_type,wrapper_path,wrapper_status,run_dir,timeout_sec,exit_code,timeout,result_status_prelim,start_iso,end_iso,elapsed_sec,notes\n' > "${manifest_out}"

if [[ "${SOURCE_ENV}" == "1" && -f "${GPGPUSIM_ROOT}/setup_environment" ]]; then
  set +u
  # shellcheck disable=SC1091
  source "${GPGPUSIM_ROOT}/setup_environment" release
  set -u
fi

abs_path() {
  local p="$1"
  if [[ "${p}" = /* ]]; then
    printf '%s\n' "${p}"
  else
    printf '%s/%s\n' "${GPGPUSIM_ROOT}" "${p}"
  fi
}

csv_quote() {
  local value="${1//$'\n'/ }"
  value="${value//\"/\"\"}"
  printf '"%s"' "${value}"
}

record_row() {
  local run_id="$1" config_id="$2" config_path="$3" paper_id="$4" paper_name="$5" paper_type="$6" wrapper_path="$7" wrapper_status="$8" run_dir="$9" timeout_sec="${10}" exit_code="${11}" timeout_flag="${12}" prelim="${13}" start_iso="${14}" end_iso="${15}" elapsed="${16}" notes="${17}"
  {
    csv_quote "${run_id}"; printf ','
    csv_quote "${config_id}"; printf ','
    csv_quote "${config_path}"; printf ','
    csv_quote "${paper_id}"; printf ','
    csv_quote "${paper_name}"; printf ','
    csv_quote "${paper_type}"; printf ','
    csv_quote "${wrapper_path}"; printf ','
    csv_quote "${wrapper_status}"; printf ','
    csv_quote "${run_dir}"; printf ','
    csv_quote "${timeout_sec}"; printf ','
    csv_quote "${exit_code}"; printf ','
    csv_quote "${timeout_flag}"; printf ','
    csv_quote "${prelim}"; printf ','
    csv_quote "${start_iso}"; printf ','
    csv_quote "${end_iso}"; printf ','
    csv_quote "${elapsed}"; printf ','
    csv_quote "${notes}"; printf '\n'
  } >> "${manifest_out}"
}

run_count=0
while IFS=',' read -r config_id config_path config_role enabled config_notes; do
  [[ "${config_id}" == "config_id" ]] && continue
  [[ -n "${config_id}" ]] || continue
  if [[ -n "${FILTER_CONFIG_ID}" && "${config_id}" != "${FILTER_CONFIG_ID}" ]]; then
    continue
  fi
  if [[ "${enabled}" != "1" ]]; then
    continue
  fi
  config_abs="$(abs_path "${config_path}")"

  while IFS=',' read -r paper_id paper_name paper_type availability wrapper_path wrapper_status build_required build_command run_working_dir run_command input_size row_timeout dry_run_status workload_notes; do
    [[ "${paper_id}" == "paper_id" ]] && continue
    [[ -n "${paper_id}" ]] || continue
    if [[ -n "${FILTER_PAPER_ID}" && "${paper_id}" != "${FILTER_PAPER_ID}" ]]; then
      continue
    fi
    if [[ "${ONLY_READY}" == "1" && "${wrapper_status}" != "ready" ]]; then
      continue
    fi
    if [[ "${wrapper_status}" == ready && "${RUN_READY}" != "1" ]]; then
      continue
    fi
    if [[ "${wrapper_status}" != ready && "${RUN_PLACEHOLDERS}" != "1" ]]; then
      run_dir="${OUTDIR}/runs/${config_id}/${paper_id}"
      mkdir -p "${run_dir}"
      record_row "${config_id}__${paper_id}" "${config_id}" "${config_abs}" "${paper_id}" "${paper_name}" "${paper_type}" "${wrapper_path}" "${wrapper_status}" "${run_dir}" "${TIMEOUT_SEC}" "" "0" "skipped_placeholder" "" "" "0" "${workload_notes}"
      continue
    fi
    if [[ "${MAX_RUNS}" -gt 0 && "${run_count}" -ge "${MAX_RUNS}" ]]; then
      run_dir="${OUTDIR}/runs/${config_id}/${paper_id}"
      mkdir -p "${run_dir}"
      record_row "${config_id}__${paper_id}" "${config_id}" "${config_abs}" "${paper_id}" "${paper_name}" "${paper_type}" "${wrapper_path}" "${wrapper_status}" "${run_dir}" "${TIMEOUT_SEC}" "" "0" "skipped_max_runs" "" "" "0" "${workload_notes}"
      continue
    fi

    wrapper_abs="$(abs_path "${wrapper_path}")"
    run_id="${config_id}__${paper_id}"
    run_dir="${OUTDIR}/runs/${config_id}/${paper_id}"
    mkdir -p "${run_dir}"
    if [[ -d "${config_abs}" ]]; then
      cp -f "${config_abs}/gpgpusim.config" "${run_dir}/" 2>/dev/null || true
      cp -f "${config_abs}/config_volta_islip.icnt" "${run_dir}/" 2>/dev/null || true
    fi

    if [[ ! -x "${wrapper_abs}" ]]; then
      printf 'missing executable wrapper: %s\n' "${wrapper_abs}" > "${run_dir}/combined.log"
      record_row "${run_id}" "${config_id}" "${config_abs}" "${paper_id}" "${paper_name}" "${paper_type}" "${wrapper_path}" "${wrapper_status}" "${run_dir}" "${TIMEOUT_SEC}" "127" "0" "wrapper_missing" "$(date -Iseconds)" "$(date -Iseconds)" "0" "${workload_notes}"
      continue
    fi

    if [[ "${USE_MANIFEST_TIMEOUT}" == "1" && -n "${row_timeout}" ]]; then
      actual_timeout="${row_timeout}"
    else
      actual_timeout="${TIMEOUT_SEC}"
    fi
    if [[ "${DRY_RUN_ONLY}" == "1" ]]; then
      cmd=( "${wrapper_abs}" "--dry-run" )
    else
      cmd=( "${wrapper_abs}" )
    fi
    printf '%q ' "${cmd[@]}" > "${run_dir}/command.txt"
    printf '\n' >> "${run_dir}/command.txt"
    {
      printf 'MASCAR_RUN_DIR=%s\n' "${run_dir}"
      printf 'MASCAR_CONFIG_DIR=%s\n' "${config_abs}"
      printf 'MASCAR_TIMEOUT_SEC=%s\n' "${actual_timeout}"
      printf 'GPGPUSIM_ROOT=%s\n' "${GPGPUSIM_ROOT}"
      printf 'GPGPU_WORKLOAD_ROOT=%s\n' "${GPGPU_WORKLOAD_ROOT}"
      printf 'DRY_RUN_ONLY=%s\n' "${DRY_RUN_ONLY}"
    } > "${run_dir}/env.txt"

    start_iso="$(date -Iseconds)"
    start_sec="$(date +%s)"
    set +e
    if [[ "${DRY_RUN_ONLY}" == "1" ]]; then
      MASCAR_RUN_DIR="${run_dir}" \
      MASCAR_CONFIG_DIR="${config_abs}" \
      MASCAR_TIMEOUT_SEC="${actual_timeout}" \
      GPGPUSIM_ROOT="${GPGPUSIM_ROOT}" \
      GPGPU_WORKLOAD_ROOT="${GPGPU_WORKLOAD_ROOT}" \
      "${cmd[@]}" > "${run_dir}/stdout.log" 2> "${run_dir}/stderr.log"
    else
      MASCAR_RUN_DIR="${run_dir}" \
      MASCAR_CONFIG_DIR="${config_abs}" \
      MASCAR_TIMEOUT_SEC="${actual_timeout}" \
      GPGPUSIM_ROOT="${GPGPUSIM_ROOT}" \
      GPGPU_WORKLOAD_ROOT="${GPGPU_WORKLOAD_ROOT}" \
      timeout "${actual_timeout}" "${cmd[@]}" > "${run_dir}/stdout.log" 2> "${run_dir}/stderr.log"
    fi
    exit_code=$?
    set -e
    end_iso="$(date -Iseconds)"
    end_sec="$(date +%s)"
    elapsed=$((end_sec - start_sec))
    cat "${run_dir}/stdout.log" "${run_dir}/stderr.log" > "${run_dir}/combined.log"
    printf '%s\n' "${exit_code}" > "${run_dir}/exit_code.txt"
    timeout_flag=0
    prelim="completed_nonzero_or_error"
    if [[ "${exit_code}" == "77" ]]; then
      prelim="wrapper_unavailable"
    elif [[ "${exit_code}" == "124" || "${exit_code}" == "137" ]]; then
      timeout_flag=1
      prelim="timeout"
    elif [[ "${exit_code}" == "0" ]]; then
      prelim="completed_exit0"
    fi
    record_row "${run_id}" "${config_id}" "${config_abs}" "${paper_id}" "${paper_name}" "${paper_type}" "${wrapper_path}" "${wrapper_status}" "${run_dir}" "${actual_timeout}" "${exit_code}" "${timeout_flag}" "${prelim}" "${start_iso}" "${end_iso}" "${elapsed}" "${workload_notes}"
    run_count=$((run_count + 1))
  done < "${WORKLOAD_MANIFEST}"
done < "${CONFIG_MATRIX}"

printf 'outdir=%s\n' "${OUTDIR}"
printf 'run_manifest=%s\n' "${manifest_out}"
