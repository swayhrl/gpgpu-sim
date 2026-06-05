#!/usr/bin/env bash
set -euo pipefail
repo_root="$(cd "$(dirname "${BASH_SOURCE[0]}")/../../../../.." && pwd)"
w17_dir="${repo_root}/experiments/paper-mascar/workloads/matrix/W17"
results_root="${repo_root}/experiments/paper-mascar/workloads/results/W17"
manifest="${w17_dir}/w17_command_manifest_updated.csv"
W17_RUN_ACTUAL="${W17_RUN_ACTUAL:-1}"
W17_MAX_ACTUAL_RUNS="${W17_MAX_ACTUAL_RUNS:-10}"
W17_TIMEOUT_SEC="${W17_TIMEOUT_SEC:-1200}"
W17_ONLY_PAPER_ID="${W17_ONLY_PAPER_ID:-}"
W17_OUTDIR="${W17_OUTDIR:-${results_root}/w17c_smoke_$(date '+%Y%m%d_%H%M%S')}"
GPGPUSIM_ROOT="${GPGPUSIM_ROOT:-${repo_root}}"
GPGPU_WORKLOAD_ROOT="${GPGPU_WORKLOAD_ROOT:-/workspace/repos/gpgpu-workloads}"
MASCAR_CONFIG_DIR="${MASCAR_CONFIG_DIR:-${repo_root}/configs/hrl-repro/SM7_QV100_mascar_baseline_off}"
mkdir -p "${W17_OUTDIR}"
results_csv="${results_root}/w17c_smoke_results.csv"
printf 'paper_id,mode,wrapper_path,w17_update_status,phase_mapping_status,exit_code,timeout,status,log_path,notes\n' > "${results_csv}"

csv_quote() { local v="${1//$'\n'/ }"; v="${v//\"/\"\"}"; printf '"%s"' "$v"; }
record() {
  { csv_quote "$1"; printf ','; csv_quote "$2"; printf ','; csv_quote "$3"; printf ','; csv_quote "$4"; printf ','; csv_quote "$5"; printf ','; csv_quote "$6"; printf ','; csv_quote "$7"; printf ','; csv_quote "$8"; printf ','; csv_quote "$9"; printf ','; csv_quote "${10}"; printf '\n'; } >> "${results_csv}"
}

mapfile -t rows < <(python3 - <<PY
import csv
for r in csv.DictReader(open(${manifest@Q})):
    if ${W17_ONLY_PAPER_ID@Q} and r['paper_id'] != ${W17_ONLY_PAPER_ID@Q}:
        continue
    print('|'.join([r['paper_id'], r['wrapper_path'], r['W17_update_status'], r['phase_mapping_status']]))
PY
)
actual_count=0
for row in "${rows[@]}"; do
  IFS='|' read -r paper_id wrapper_path update_status phase_status <<< "$row"
  wrapper_abs="${repo_root}/${wrapper_path}"
  dry_log="${W17_OUTDIR}/${paper_id}_dry_run.log"
  set +e
  COMMAND_MANIFEST="${manifest}" GPGPUSIM_ROOT="${GPGPUSIM_ROOT}" GPGPU_WORKLOAD_ROOT="${GPGPU_WORKLOAD_ROOT}" "$wrapper_abs" --dry-run > "$dry_log" 2>&1
  ec=$?
  set -e
  if [[ "$ec" == "0" ]]; then status=dry_run_pass; else status=dry_run_fail; fi
  record "$paper_id" dry-run "$wrapper_path" "$update_status" "$phase_status" "$ec" 0 "$status" "$dry_log" ""
  if [[ "$W17_RUN_ACTUAL" == "1" && ( "$update_status" == "app_level_ready_phase_pending" || "$update_status" == "newly_ready" || "$update_status" == "wrapper_fixed" ) ]]; then
    if [[ "$actual_count" -ge "$W17_MAX_ACTUAL_RUNS" ]]; then
      record "$paper_id" actual "$wrapper_path" "$update_status" "$phase_status" "" 0 skipped_max_actual "" "W17_MAX_ACTUAL_RUNS reached"
      continue
    fi
    actual_log="${W17_OUTDIR}/${paper_id}_actual.log"
    run_dir="${W17_OUTDIR}/${paper_id}_run"
    mkdir -p "$run_dir"
    set +e
    COMMAND_MANIFEST="${manifest}" MASCAR_RUN_DIR="$run_dir" MASCAR_CONFIG_DIR="$MASCAR_CONFIG_DIR" MASCAR_TIMEOUT_SEC="$W17_TIMEOUT_SEC" GPGPUSIM_ROOT="$GPGPUSIM_ROOT" GPGPU_WORKLOAD_ROOT="$GPGPU_WORKLOAD_ROOT" timeout "$W17_TIMEOUT_SEC" "$wrapper_abs" > "$actual_log" 2>&1
    ec=$?
    set -e
    timeout_flag=0; [[ "$ec" == "124" || "$ec" == "137" ]] && timeout_flag=1
    if [[ "$timeout_flag" == "1" ]]; then status=timeout; elif [[ "$ec" == "0" ]]; then status=actual_pass; elif [[ "$ec" == "77" ]]; then status=unexpected_unavailable; else status=actual_nonzero; fi
    record "$paper_id" actual "$wrapper_path" "$update_status" "$phase_status" "$ec" "$timeout_flag" "$status" "$actual_log" ""
    actual_count=$((actual_count + 1))
  fi
done
printf 'results=%s\n' "$results_csv"
printf 'outdir=%s\n' "$W17_OUTDIR"
