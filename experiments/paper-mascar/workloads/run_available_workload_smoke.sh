#!/usr/bin/env bash
set -euo pipefail

script_dir="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
manifest="${COMMAND_MANIFEST:-${script_dir}/mascar_table_iii_command_manifest.csv}"
outdir="${SMOKE_OUTDIR:-${script_dir}/smoke_logs/$(date '+%Y%m%d_%H%M%S')}"
run_actual="${RUN_ACTUAL:-0}"
max_actual="${MAX_ACTUAL_RUNS:-3}"
timeout_sec="${TIMEOUT_SEC:-1200}"

mkdir -p "${outdir}"
results="${outdir}/smoke_results.csv"
printf 'paper_id,mode,exit_code,log_path,wrapper\n' > "${results}"

if [[ ! -f "${manifest}" ]]; then
  echo "missing command manifest: ${manifest}" >&2
  exit 2
fi

actual_count=0
while IFS=',' read -r paper_id paper_name paper_type availability wrapper_path wrapper_status build_required build_command run_working_dir run_command input_size row_timeout dry_run_status notes; do
  [[ "${paper_id}" == "paper_id" ]] && continue
  [[ -n "${paper_id}" ]] || continue
  wrapper="${script_dir}/wrappers/run_${paper_id}.sh"
  dry_log="${outdir}/${paper_id}.dry_run.log"
  if [[ ! -x "${wrapper}" ]]; then
    echo "missing executable wrapper: ${wrapper}" | tee "${dry_log}"
    printf '%s,%s,%s,%s,%s\n' "${paper_id}" "dry-run" "127" "${dry_log}" "${wrapper}" >> "${results}"
    continue
  fi
  set +e
  "${wrapper}" --dry-run > "${dry_log}" 2>&1
  dry_exit=$?
  set -e
  printf '%s,%s,%s,%s,%s\n' "${paper_id}" "dry-run" "${dry_exit}" "${dry_log}" "${wrapper}" >> "${results}"

  if [[ "${run_actual}" == "1" && "${wrapper_status}" == "ready" && "${actual_count}" -lt "${max_actual}" ]]; then
    actual_count=$((actual_count + 1))
    actual_log="${outdir}/${paper_id}.actual.log"
    set +e
    MASCAR_RUN_DIR="${outdir}/actual_${paper_id}" \
    MASCAR_TIMEOUT_SEC="${timeout_sec}" \
    timeout "${timeout_sec}" "${wrapper}" > "${actual_log}" 2>&1
    actual_exit=$?
    set -e
    printf '%s,%s,%s,%s,%s\n' "${paper_id}" "actual" "${actual_exit}" "${actual_log}" "${wrapper}" >> "${results}"
  fi
done < "${manifest}"

echo "Smoke results: ${results}"
