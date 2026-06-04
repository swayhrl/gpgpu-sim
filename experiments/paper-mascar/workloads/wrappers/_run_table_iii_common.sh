#!/usr/bin/env bash
set -euo pipefail

paper_id="${1:-}"
shift || true

script_dir="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
workload_dir="$(cd "${script_dir}/.." && pwd)"
repo_root="$(cd "${workload_dir}/../../.." && pwd)"
manifest="${COMMAND_MANIFEST:-${workload_dir}/mascar_table_iii_command_manifest.csv}"

usage() {
  cat <<EOF
Usage: $(basename "$0") <paper_id> [--dry-run|--print-command|--help]

Environment:
  MASCAR_RUN_DIR      Directory for wrapper logs.
  MASCAR_CONFIG_DIR   Optional GPGPU-Sim config override directory.
  MASCAR_TIMEOUT_SEC  Per-wrapper timeout in seconds.
  GPGPUSIM_ROOT       GPGPU-Sim repo root.
  GPGPU_WORKLOAD_ROOT Workload repo root.
EOF
}

if [[ -z "${paper_id}" ]]; then
  usage
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

row="$(
  awk -F',' -v id="${paper_id}" 'NR>1 && $1==id {print; found=1; exit} END {if (!found) exit 7}' "${manifest}"
)" || {
  echo "paper_id not found in command manifest: ${paper_id}" >&2
  exit 2
}

IFS=',' read -r row_id paper_name paper_type availability wrapper_path wrapper_status build_required build_command run_working_dir run_command input_size timeout_sec dry_run_status notes <<< "${row}"

timeout_sec="${MASCAR_TIMEOUT_SEC:-${timeout_sec:-1200}}"
workload_root="${GPGPU_WORKLOAD_ROOT:-/workspace/repos/gpgpu-workloads}"
gpgpusim_root="${GPGPUSIM_ROOT:-${repo_root}}"
run_working_dir="${run_working_dir//\/workspace\/repos\/gpgpu-workloads/${workload_root}}"
run_dir="${MASCAR_RUN_DIR:-${workload_dir}/wrapper_runs/$(date '+%Y%m%d_%H%M%S')}"
mkdir -p "${run_dir}"
run_dir="$(cd "${run_dir}" && pwd)"

print_identity() {
  echo "paper_id=${row_id}"
  echo "paper_name=${paper_name}"
  echo "paper_type=${paper_type}"
  echo "availability_status=${availability}"
  echo "wrapper_status=${wrapper_status}"
  echo "input_size=${input_size}"
  echo "timeout_sec=${timeout_sec}"
  echo "notes=${notes}"
}

print_command() {
  if [[ -z "${run_command}" || -z "${run_working_dir}" ]]; then
    echo "unavailable"
  else
    echo "cd ${run_working_dir} && ${run_command}"
  fi
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
  echo "recommended_action=resolve availability or phase mapping before actual Table III run"
  exit 77
fi

if [[ -z "${run_working_dir}" || -z "${run_command}" ]]; then
  print_identity
  echo "ready wrapper has no command" >&2
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
      for cfg_file in gpgpusim.config config_volta_islip.icnt; do
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
  if [[ -n "${MASCAR_CONFIG_DIR:-}" ]]; then
    config_backup_dir="$(mktemp -d "${run_dir}/config_backup.XXXXXX")"
    for cfg_file in gpgpusim.config config_volta_islip.icnt; do
      if [[ -f "${cfg_file}" ]]; then
        cp -f "${cfg_file}" "${config_backup_dir}/${cfg_file}"
      fi
      if [[ -f "${MASCAR_CONFIG_DIR}/${cfg_file}" ]]; then
        cp -f "${MASCAR_CONFIG_DIR}/${cfg_file}" "${cfg_file}"
      fi
    done
    trap restore_config EXIT
    TIMEOUT_SECONDS="${timeout_sec}" timeout "${timeout_sec}" bash -lc "${run_command}"
    restore_config
    trap - EXIT
  else
    TIMEOUT_SECONDS="${timeout_sec}" \
    timeout "${timeout_sec}" bash -lc "${run_command}"
  fi
) > "${log_path}" 2>&1
