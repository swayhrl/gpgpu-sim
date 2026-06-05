#!/usr/bin/env bash
set -euo pipefail

script_dir="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
repo_root="$(cd "${script_dir}/../../.." && pwd)"
common_runner="${repo_root}/experiments/common/gpgpusim_matrix/run_gpgpusim_matrix.sh"
SUITE_COMMAND_MANIFEST="${SUITE_COMMAND_MANIFEST:-${script_dir}/full_suite_command_manifest.csv}"
SUITE_CONFIG_MATRIX="${SUITE_CONFIG_MATRIX:-${script_dir}/full_suite_smoke_config_matrix.csv}"
SUITE_MODE="${SUITE_MODE:-dryrun_all}"
FILTER_SUITE="${FILTER_SUITE:-}"
FILTER_WORKLOAD="${FILTER_WORKLOAD:-}"
FILTER_STATUS="${FILTER_STATUS:-}"
RUN_READY="${RUN_READY:-1}"
RUN_PLACEHOLDERS="${RUN_PLACEHOLDERS:-}"
DRY_RUN_ONLY="${DRY_RUN_ONLY:-}"
MAX_RUNS="${MAX_RUNS:-0}"
TIMEOUT_SEC="${TIMEOUT_SEC:-1200}"
SOURCE_ENV="${SOURCE_ENV:-1}"
GPGPUSIM_ROOT="${GPGPUSIM_ROOT:-${repo_root}}"
GPGPU_WORKLOAD_ROOT="${GPGPU_WORKLOAD_ROOT:-/workspace/repos/gpgpu-workloads}"
SUITE_OUTDIR="${SUITE_OUTDIR:-${repo_root}/experiments/suites/results/W20/run_${SUITE_MODE}_$(date '+%Y%m%d_%H%M%S')}"

case "${SUITE_MODE}" in
  dryrun_all)
    DRY_RUN_ONLY="${DRY_RUN_ONLY:-1}"; RUN_PLACEHOLDERS="${RUN_PLACEHOLDERS:-1}" ;;
  dryrun_ready)
    DRY_RUN_ONLY="${DRY_RUN_ONLY:-1}"; RUN_PLACEHOLDERS="${RUN_PLACEHOLDERS:-0}" ;;
  smoke_ready|smoke_sample|smoke_by_filter)
    DRY_RUN_ONLY="${DRY_RUN_ONLY:-0}"; RUN_PLACEHOLDERS="${RUN_PLACEHOLDERS:-0}" ;;
  *) echo "unknown SUITE_MODE=${SUITE_MODE}" >&2; exit 2 ;;
esac

mkdir -p "${SUITE_OUTDIR}/generated_wrappers"
workload_manifest="${SUITE_OUTDIR}/generated_workload_manifest.csv"
command_snapshot="${SUITE_OUTDIR}/suite_command_manifest.snapshot.csv"
config_snapshot="${SUITE_OUTDIR}/suite_config_matrix.snapshot.csv"
env_snapshot="${SUITE_OUTDIR}/env_snapshot.txt"
cp -f "${SUITE_COMMAND_MANIFEST}" "${command_snapshot}"
cp -f "${SUITE_CONFIG_MATRIX}" "${config_snapshot}"
{
  printf 'SUITE_MODE=%s\n' "${SUITE_MODE}"
  printf 'FILTER_SUITE=%s\n' "${FILTER_SUITE}"
  printf 'FILTER_WORKLOAD=%s\n' "${FILTER_WORKLOAD}"
  printf 'FILTER_STATUS=%s\n' "${FILTER_STATUS}"
  printf 'RUN_READY=%s\n' "${RUN_READY}"
  printf 'RUN_PLACEHOLDERS=%s\n' "${RUN_PLACEHOLDERS}"
  printf 'DRY_RUN_ONLY=%s\n' "${DRY_RUN_ONLY}"
  printf 'MAX_RUNS=%s\n' "${MAX_RUNS}"
  printf 'TIMEOUT_SEC=%s\n' "${TIMEOUT_SEC}"
  printf 'SOURCE_ENV=%s\n' "${SOURCE_ENV}"
  printf 'GPGPUSIM_ROOT=%s\n' "${GPGPUSIM_ROOT}"
  printf 'GPGPU_WORKLOAD_ROOT=%s\n' "${GPGPU_WORKLOAD_ROOT}"
} > "${env_snapshot}"

python3 - "${SUITE_COMMAND_MANIFEST}" "${workload_manifest}" "${SUITE_OUTDIR}/generated_wrappers" "${SUITE_MODE}" "${FILTER_SUITE}" "${FILTER_WORKLOAD}" "${FILTER_STATUS}" "${RUN_PLACEHOLDERS}" <<'PY'
import csv, os, stat, sys
from pathlib import Path
src, out_manifest, wrapper_dir, mode, filter_suite, filter_workload, filter_status, run_placeholders = sys.argv[1:]
wrapper_dir = Path(wrapper_dir)
rows = list(csv.DictReader(open(src)))
fields = ['paper_id','paper_name','paper_type','availability','wrapper_path','wrapper_status','build_required','build_command','run_working_dir','run_command','input_size','timeout_sec','dry_run_status','notes']
out_rows=[]
for r in rows:
    app_id = r['app_id']
    if filter_suite and r['suite_id'] != filter_suite:
        continue
    if filter_workload and app_id != filter_workload:
        continue
    if filter_status and r['availability_status'] != filter_status:
        continue
    is_ready = r['current_ready'] == '1'
    if mode in {'dryrun_ready','smoke_ready','smoke_sample'} and not is_ready:
        continue
    if mode == 'smoke_by_filter' and not is_ready and run_placeholders != '1':
        continue
    wrapper = wrapper_dir / f'{app_id}.sh'
    run_command = r['run_command'] if r['run_command'] != 'unknown' else ''
    script = f'''#!/usr/bin/env bash
set -euo pipefail
mode="run"
case "${{1:-}}" in
  --help|-h) echo "W20 generated wrapper for {app_id}"; exit 0 ;;
  --dry-run) mode="dry-run" ;;
  --print-command) mode="print-command" ;;
  "") ;;
  *) echo "unknown argument: $1" >&2; exit 2 ;;
esac
if [[ "${{mode}}" == "print-command" ]]; then
  printf '%s\n' {run_command!r}
  exit 0
fi
if [[ "${{mode}}" == "dry-run" ]]; then
  printf 'app_id=%s\n' {app_id!r}
  printf 'suite_id=%s\n' {r['suite_id']!r}
  printf 'availability_status=%s\n' {r['availability_status']!r}
  printf 'current_ready=%s\n' {r['current_ready']!r}
  printf 'command=%s\n' {run_command!r}
  exit 0
fi
if [[ {r['current_ready']!r} != "1" ]]; then
  echo "W20 placeholder/unready row is not actual-run by default: {app_id}" >&2
  exit 77
fi
if [[ -z {run_command!r} ]]; then
  echo "missing run command for {app_id}" >&2
  exit 77
fi
bash -lc {run_command!r}
'''
    wrapper.write_text(script)
    wrapper.chmod(wrapper.stat().st_mode | stat.S_IXUSR | stat.S_IXGRP | stat.S_IXOTH)
    status = 'ready' if is_ready else 'placeholder_unavailable'
    out_rows.append({
        'paper_id': app_id,
        'paper_name': r.get('app_name') or app_id,
        'paper_type': r['suite_id'],
        'availability': r['availability_status'],
        'wrapper_path': str(wrapper),
        'wrapper_status': status,
        'build_required': 'no',
        'build_command': 'unknown',
        'run_working_dir': r['source_path'],
        'run_command': run_command,
        'input_size': r['input_scale'],
        'timeout_sec': r['default_timeout_sec'],
        'dry_run_status': 'available',
        'notes': r['notes'],
    })
with open(out_manifest, 'w', newline='') as f:
    w = csv.DictWriter(f, fieldnames=fields, lineterminator='\n')
    w.writeheader(); w.writerows(out_rows)
print(f'generated_rows={len(out_rows)}')
print(f'generated_manifest={out_manifest}')
PY

export CONFIG_MATRIX="${SUITE_CONFIG_MATRIX}"
export WORKLOAD_MANIFEST="${workload_manifest}"
export OUTDIR="${SUITE_OUTDIR}"
export TIMEOUT_SEC MAX_RUNS RUN_PLACEHOLDERS RUN_READY DRY_RUN_ONLY SOURCE_ENV GPGPUSIM_ROOT GPGPU_WORKLOAD_ROOT
export USE_MANIFEST_TIMEOUT=1
export ONLY_READY=0
if [[ "${SUITE_MODE}" == "smoke_sample" && "${MAX_RUNS}" == "0" ]]; then
  export MAX_RUNS=3
fi
bash "${common_runner}"
printf 'suite_outdir=%s\n' "${SUITE_OUTDIR}"
