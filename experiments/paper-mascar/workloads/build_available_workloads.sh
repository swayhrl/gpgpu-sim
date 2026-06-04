#!/usr/bin/env bash
set -euo pipefail

script_dir="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
manifest="${WORKLOAD_MANIFEST:-${script_dir}/mascar_table_iii_command_manifest.csv}"
outdir="${script_dir}/build_logs/$(date '+%Y%m%d_%H%M%S')"
timeout_sec="${BUILD_TIMEOUT_SEC:-1200}"
max_builds="${MAX_BUILDS:-10}"
dry_run="${DRY_RUN:-0}"

mkdir -p "${outdir}"
results="${outdir}/build_results.csv"
printf 'paper_id,exit_code,log_path,command\n' > "${results}"

if [[ ! -f "${manifest}" ]]; then
  echo "missing command manifest: ${manifest}" >&2
  exit 2
fi

count=0
while IFS=',' read -r paper_id paper_name paper_type availability wrapper_path wrapper_status build_required build_command run_working_dir run_command input_size row_timeout dry_run_status notes; do
  [[ "${paper_id}" == "paper_id" ]] && continue
  [[ -n "${paper_id}" ]] || continue
  if [[ "${build_required}" != "yes" || -z "${build_command}" ]]; then
    continue
  fi
  count=$((count + 1))
  if [[ "${count}" -gt "${max_builds}" ]]; then
    echo "MAX_BUILDS reached: ${max_builds}"
    break
  fi
  log_path="${outdir}/${paper_id}.log"
  echo "[BUILD] ${paper_id}: ${build_command}"
  if [[ "${dry_run}" == "1" ]]; then
    printf '%s,%s,%s,%s\n' "${paper_id}" "dry_run" "${log_path}" "${build_command}" >> "${results}"
    continue
  fi
  set +e
  (
    cd "${run_working_dir:-${script_dir}}" || exit 97
    timeout "${timeout_sec}" bash -lc "${build_command}"
  ) > "${log_path}" 2>&1
  exit_code=$?
  set -e
  printf '%s,%s,%s,%s\n' "${paper_id}" "${exit_code}" "${log_path}" "${build_command}" >> "${results}"
done < "${manifest}"

if [[ "${count}" -eq 0 ]]; then
  echo "No buildable workloads were found in ${manifest}."
fi
echo "Build results: ${results}"
