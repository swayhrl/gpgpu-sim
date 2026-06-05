#!/usr/bin/env bash
set -euo pipefail
script_dir="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
repo_root="$(cd "${script_dir}/../../../../.." && pwd)"
strategy="${MASCAR_M3_DIAG_STRATEGY:-1}"
case "${1:-}" in
  --help|-h) echo "Usage: MASCAR_M3_DIAG_STRATEGY=1..5 $0 [--dry-run|--print-command]"; exit 0;;
  --dry-run) mode=dry;;
  --print-command) mode=print;;
  "") mode=run;;
  *) echo "unknown arg: $1" >&2; exit 2;;
esac
case "${strategy}" in
  1) args="32 128 262144 512 16 4 97";;
  2) args="32 128 262144 512 16 4 97";;
  3) args="64 128 262144 512 24 4 97";;
  4) args="32 128 262144 128 24 8 97";;
  5) args="32 128 524288 512 24 4 257";;
  *) echo "invalid strategy: ${strategy}" >&2; exit 2;;
esac
if [[ "${mode}" != run ]]; then
  echo "cd ${script_dir} && ./mascar_m3_diag ${args}"
  exit 0
fi
if [[ ! -x "${script_dir}/mascar_m3_diag" ]]; then
  echo "binary missing: ${script_dir}/mascar_m3_diag" >&2
  exit 77
fi
run_dir="${MASCAR_RUN_DIR:-${script_dir}/runs/$(date '+%Y%m%d_%H%M%S')}"
mkdir -p "${run_dir}"
(
  cd "${script_dir}"
  if [[ -f "${repo_root}/setup_environment" ]]; then
    set +u
    source "${repo_root}/setup_environment" release
    set -u
  fi
  backup=""
  restore_config() {
    if [[ -n "${backup}" && -d "${backup}" ]]; then
      for cfg_file in gpgpusim.config config_volta_islip.icnt; do
        if [[ -f "${backup}/${cfg_file}" ]]; then cp -f "${backup}/${cfg_file}" "${cfg_file}"; else rm -f "${cfg_file}"; fi
      done
      rm -rf "${backup}"
    fi
  }
  if [[ -n "${MASCAR_CONFIG_DIR:-}" ]]; then
    backup="$(mktemp -d "${run_dir}/micro_config_backup.XXXXXX")"
    for cfg_file in gpgpusim.config config_volta_islip.icnt; do
      [[ -f "${cfg_file}" ]] && cp -f "${cfg_file}" "${backup}/${cfg_file}"
      [[ -f "${MASCAR_CONFIG_DIR}/${cfg_file}" ]] && cp -f "${MASCAR_CONFIG_DIR}/${cfg_file}" "${cfg_file}"
    done
    trap restore_config EXIT
  fi
  ./mascar_m3_diag ${args}
  restore_config
  trap - EXIT
)
