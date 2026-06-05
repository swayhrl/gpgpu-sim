#!/usr/bin/env bash
set -euo pipefail

paper_id="${1:-}"
shift || true
script_dir="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
repo_root="$(cd "${script_dir}/../../../.." && pwd)"
manifest="${COMMAND_MANIFEST:-${script_dir}/w16_energy_workload_manifest.csv}"

usage() {
  cat <<USAGE
Usage: $(basename "$0") <paper_id> [--dry-run|--print-command|--help]
USAGE
}

if [[ -z "${paper_id}" ]]; then
  usage >&2
  exit 2
fi

mode="run"
case "${1:-}" in
  --help|-h)
    usage
    exit 0
    ;;
  --dry-run)
    mode="dry-run"
    ;;
  --print-command)
    mode="print-command"
    ;;
  "")
    ;;
  *)
    echo "unknown argument: $1" >&2
    usage >&2
    exit 2
    ;;
esac

if [[ ! -f "${manifest}" ]]; then
  echo "missing command manifest: ${manifest}" >&2
  exit 2
fi

row="$(awk -F',' -v id="${paper_id}" 'NR>1 && $1==id {print; found=1; exit} END {if (!found) exit 7}' "${manifest}")" || {
  echo "paper_id not found in command manifest: ${paper_id}" >&2
  exit 2
}
IFS=',' read -r row_id paper_name paper_type availability wrapper_path wrapper_status build_required build_command run_working_dir run_command input_size timeout_sec dry_run_status notes <<< "${row}"

timeout_sec="${MASCAR_TIMEOUT_SEC:-${timeout_sec:-1800}}"
workload_root="${GPGPU_WORKLOAD_ROOT:-/workspace/repos/gpgpu-workloads}"
gpgpusim_root="${GPGPUSIM_ROOT:-${repo_root}}"
run_working_dir="${run_working_dir//\/workspace\/repos\/gpgpu-workloads/${workload_root}}"
run_dir="${MASCAR_RUN_DIR:-${script_dir}/wrapper_runs/$(date '+%Y%m%d_%H%M%S')}"
mkdir -p "${run_dir}"
run_dir="$(cd "${run_dir}" && pwd)"
exec_dir_for_config="$(printf '%s\n' "${run_command}" | sed -n 's/^cd \([^ ]*\).*/\1/p')"

print_identity() {
  echo "paper_id=${row_id}"
  echo "paper_name=${paper_name}"
  echo "wrapper_status=${wrapper_status}"
  echo "input_size=${input_size}"
  echo "timeout_sec=${timeout_sec}"
  echo "notes=${notes}"
}

print_command() {
  echo "cd ${run_working_dir} && ${run_command}"
}

if [[ "${mode}" == "dry-run" ]]; then
  print_identity
  echo -n "command="
  print_command
  exit 0
fi
if [[ "${mode}" == "print-command" ]]; then
  print_command
  exit 0
fi
if [[ "${wrapper_status}" != "ready" ]]; then
  print_identity
  exit 77
fi
if [[ ! -d "${run_working_dir}" ]]; then
  print_identity
  echo "missing run working dir: ${run_working_dir}" >&2
  exit 77
fi

log_path="${run_dir}/${row_id}.log"
echo "wrapper_log=${log_path}"
(
  cd "${run_working_dir}"
  restore_config() {
    if [[ -n "${config_backup_dir:-}" && -d "${config_backup_dir}" ]]; then
      for cfg_file in gpgpusim.config config_volta_islip.icnt accelwattch_ptx_sim.xml accelwattch_sass_sim.xml accelwattch_sass_sim_alt.xml; do
        if [[ -f "${config_backup_dir}/${cfg_file}" ]]; then
          cp -f "${config_backup_dir}/${cfg_file}" "${cfg_file}"
        else
          rm -f "${cfg_file}"
        fi
      done
      rm -rf "${config_backup_dir}"
    fi
  }
  if [[ -f "${gpgpusim_root}/setup_environment" ]]; then
    set +u
    # shellcheck disable=SC1091
    source "${gpgpusim_root}/setup_environment" release
    set -u
  fi
  rm -f accelwattch_power_report.log
  if [[ -n "${exec_dir_for_config}" && -d "${exec_dir_for_config}" ]]; then
    rm -f "${exec_dir_for_config}/accelwattch_power_report.log"
  fi
  marker_file="${run_dir}/power_report_start.marker"
  : > "${marker_file}"
  if [[ -n "${MASCAR_CONFIG_DIR:-}" ]]; then
    config_backup_dir="$(mktemp -d "${run_dir}/config_backup.XXXXXX")"
    for cfg_file in gpgpusim.config config_volta_islip.icnt accelwattch_ptx_sim.xml accelwattch_sass_sim.xml accelwattch_sass_sim_alt.xml; do
      if [[ -f "${cfg_file}" ]]; then
        cp -f "${cfg_file}" "${config_backup_dir}/${cfg_file}"
      fi
      if [[ -f "${MASCAR_CONFIG_DIR}/${cfg_file}" ]]; then
        cp -f "${MASCAR_CONFIG_DIR}/${cfg_file}" "${cfg_file}"
      fi
    done
    if [[ -n "${exec_dir_for_config}" && -d "${exec_dir_for_config}" ]]; then
      for xml_file in "${MASCAR_CONFIG_DIR}"/*.xml; do
        [[ -f "${xml_file}" ]] || continue
        cp -f "${xml_file}" "${exec_dir_for_config}/"
      done
    fi
    export GPGPUSIM_CONFIG_OVERRIDE="${MASCAR_CONFIG_DIR}"
    trap restore_config EXIT
  fi
  set +e
  TIMEOUT_SECONDS="${timeout_sec}" timeout "${timeout_sec}" bash -lc "${run_command}"
  exit_code=$?
  set -e
  if [[ -f accelwattch_power_report.log ]]; then
    cp -f accelwattch_power_report.log "${run_dir}/accelwattch_power_report.log"
    echo "power_report_status=found"
  elif [[ -n "${exec_dir_for_config}" && -f "${exec_dir_for_config}/accelwattch_power_report.log" ]]; then
    cp -f "${exec_dir_for_config}/accelwattch_power_report.log" "${run_dir}/accelwattch_power_report.log"
    echo "power_report_status=found"
  else
    latest_report="$(find "${workload_root}" -name accelwattch_power_report.log -type f -newer "${marker_file}" -printf '%T@ %p\n' 2>/dev/null | sort -nr | head -n 1 | cut -d' ' -f2-)"
    if [[ -n "${latest_report}" && -f "${latest_report}" ]]; then
      cp -f "${latest_report}" "${run_dir}/accelwattch_power_report.log"
      echo "power_report_status=found_via_workload_root_scan"
    else
      echo "power_report_status=missing" | tee "${run_dir}/power_report_status.txt"
    fi
  fi
  restore_config
  trap - EXIT
  exit "${exit_code}"
) > "${log_path}" 2>&1
