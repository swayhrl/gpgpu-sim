#!/usr/bin/env bash
set -euo pipefail

script_dir="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
manifest="${W19_COMMAND_MANIFEST:-${script_dir}/suite_command_manifest.csv}"
mode="dry-run"
ready_only=1
filter_id=""
limit=0
results_prefix="${W19_RESULTS_PREFIX:-w19_smoke}"
raw_root="${W19_RAW_ROOT:-/workspace/tmp}"
stamp="$(date '+%Y%m%d_%H%M%S')"
log_root="${W19_LOG_ROOT:-${raw_root}/mascar_w19_smoke_logs_${stamp}}"

usage() {
  cat <<EOF
Usage: $(basename "$0") [--dry-run|--smoke] [--workload ID] [--limit N] [--print-command ID]

Runs or dry-runs W19 Rodinia/Parboil unified suite wrappers.
Only wrapper_status=ready rows are eligible by default.
EOF
}

while [[ $# -gt 0 ]]; do
  case "$1" in
    --help|-h) usage; exit 0 ;;
    --dry-run) mode="dry-run"; shift ;;
    --smoke) mode="smoke"; shift ;;
    --workload) filter_id="$2"; shift 2 ;;
    --limit) limit="$2"; shift 2 ;;
    --print-command)
      filter_id="$2"; mode="print-command"; shift 2 ;;
    *) echo "unknown argument: $1" >&2; usage >&2; exit 2 ;;
  esac
done

if [[ ! -f "${manifest}" ]]; then
  echo "missing manifest: ${manifest}" >&2
  exit 2
fi

mkdir -p "${script_dir}" "${log_root}"
run_manifest="${script_dir}/${results_prefix}_run_manifest.csv"
results_csv="${script_dir}/${results_prefix}_results.csv"
summary_md="${script_dir}/${results_prefix}_summary.md"
status_matrix="${script_dir}/${results_prefix}_status_matrix.csv"
printf 'run_id,suite_workload_id,suite,benchmark,availability_status,wrapper_status,run_dir,log_path,exit_code,timeout,result_status_prelim\n' > "${run_manifest}"
printf 'run_id,suite_workload_id,suite,benchmark,availability_status,exit_code,timeout,classification,has_stats,explicit_pass,gpu_tot_sim_cycle,log_path\n' > "${results_csv}"

row_stream() {
  python3 - "$manifest" "$filter_id" "$limit" <<'PY'
import csv, sys
manifest, filter_id, limit = sys.argv[1], sys.argv[2], int(sys.argv[3])
count=0
fields=['suite_workload_id','suite','benchmark','availability_status','wrapper_status','run_working_dir','run_command','timeout_sec']
with open(manifest, newline='') as f:
    for r in csv.DictReader(f):
        if r.get('wrapper_status') != 'ready':
            continue
        if filter_id and r.get('suite_workload_id') != filter_id:
            continue
        print('\t'.join(r.get(k,'').replace('\t',' ') for k in fields))
        count += 1
        if limit and count >= limit:
            break
PY
}

if [[ "${mode}" == "print-command" ]]; then
  row_stream | awk -F'\t' '{print $7}'
  exit 0
fi

idx=0
while IFS=$'\t' read -r suite_workload_id suite benchmark availability_status wrapper_status run_working_dir run_command timeout_sec; do
  [[ -z "${suite_workload_id}" ]] && continue
  idx=$((idx+1))
  run_id="${results_prefix}_${suite_workload_id}"
  run_dir="${log_root}/${run_id}"
  log_path="${run_dir}/run.log"
  mkdir -p "${run_dir}"
  exit_code=0
  timeout_flag=0
  prelim="completed"
  if [[ "${mode}" == "dry-run" ]]; then
    {
      echo "suite_workload_id=${suite_workload_id}"
      echo "availability_status=${availability_status}"
      echo "run_working_dir=${run_working_dir}"
      echo "command=${run_command}"
    } > "${log_path}"
    prelim="dry_run"
  else
    timeout "${timeout_sec:-1800}" bash -lc "${run_command}" > "${log_path}" 2>&1 || exit_code=$?
    if [[ "${exit_code}" == "124" ]]; then timeout_flag=1; prelim="timeout"; elif [[ "${exit_code}" != "0" ]]; then prelim="nonzero"; fi
  fi
  has_stats=0
  explicit_pass=0
  gpu_tot_sim_cycle=""
  if grep -Eq '\bgpu_tot_sim_cycle\s*=' "${log_path}"; then
    has_stats=1
    gpu_tot_sim_cycle="$(grep -E '\bgpu_tot_sim_cycle\s*=' "${log_path}" | tail -n 1 | sed -E 's/.*=\s*([0-9.]+).*/\1/')"
  fi
  if grep -Eiq '\b(PASS|PASSED|Test Passed|passed verification)\b' "${log_path}"; then explicit_pass=1; fi
  classification="unknown"
  if [[ "${mode}" == "dry-run" ]]; then classification="dry_run_ready";
  elif [[ "${timeout_flag}" == "1" ]]; then classification="timeout";
  elif [[ "${exit_code}" != "0" && "${has_stats}" == "1" ]]; then classification="nonzero_with_stats";
  elif [[ "${exit_code}" != "0" ]]; then classification="nonzero_no_stats";
  elif [[ "${explicit_pass}" == "1" ]]; then classification="completed_explicit_pass";
  elif [[ "${has_stats}" == "1" ]]; then classification="completed_no_explicit_pass";
  else classification="exit0_no_stats"; fi
  printf '%s,%s,%s,%s,%s,%s,%s,%s,%s,%s,%s\n' \
    "${run_id}" "${suite_workload_id}" "${suite}" "${benchmark}" "${availability_status}" "${wrapper_status}" "${run_dir}" "${log_path}" "${exit_code}" "${timeout_flag}" "${prelim}" >> "${run_manifest}"
  printf '%s,%s,%s,%s,%s,%s,%s,%s,%s,%s,%s,%s\n' \
    "${run_id}" "${suite_workload_id}" "${suite}" "${benchmark}" "${availability_status}" "${exit_code}" "${timeout_flag}" "${classification}" "${has_stats}" "${explicit_pass}" "${gpu_tot_sim_cycle}" "${log_path}" >> "${results_csv}"
done < <(row_stream)

python3 - "$results_csv" "$summary_md" "$status_matrix" <<'PY'
import csv, sys, collections
results, summary, matrix = sys.argv[1:4]
rows=list(csv.DictReader(open(results)))
counts=collections.Counter(r['classification'] for r in rows)
with open(summary,'w') as f:
    f.write('# W19 Smoke Summary\n\n')
    f.write(f'rows: {len(rows)}\n\n')
    for k,v in sorted(counts.items()): f.write(f'- {k}: {v}\n')
with open(matrix,'w',newline='') as f:
    fields=['suite_workload_id','suite','benchmark','availability_status','classification']
    w=csv.DictWriter(f, fieldnames=fields, lineterminator='\n')
    w.writeheader()
    for r in rows: w.writerow({k:r[k] for k in fields})
print(f'w19_rows={len(rows)}')
print(f'w19_log_root={str(__import__("pathlib").Path(rows[0]["log_path"]).parents[1]) if rows else ""}')
PY
