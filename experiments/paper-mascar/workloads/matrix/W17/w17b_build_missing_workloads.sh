#!/usr/bin/env bash
set -euo pipefail

repo_root="$(cd "$(dirname "${BASH_SOURCE[0]}")/../../../../.." && pwd)"
w17_dir="${repo_root}/experiments/paper-mascar/workloads/matrix/W17"
results_dir="${repo_root}/experiments/paper-mascar/workloads/results/W17"
gap_csv="${w17_dir}/w17a_gap_matrix.csv"
mkdir -p "${results_dir}"

W17_BUILD_TIMEOUT_SEC="${W17_BUILD_TIMEOUT_SEC:-1200}"
W17_MAX_BUILDS="${W17_MAX_BUILDS:-0}"
W17_DRY_RUN="${W17_DRY_RUN:-0}"
W17_ONLY_PAPER_ID="${W17_ONLY_PAPER_ID:-}"
W17_LOG_DIR="${W17_LOG_DIR:-${results_dir}/build_logs/$(date '+%Y%m%d_%H%M%S')}"
mkdir -p "${W17_LOG_DIR}"

build_results="${results_dir}/w17b_build_results.csv"
binary_results="${results_dir}/w17b_binary_recovery_results.csv"
data_results="${results_dir}/w17b_data_availability_results.csv"
printf 'paper_id,source_path,build_command,build_attempt,exit_code,timeout,binary_path,build_status,log_path,notes\n' > "${build_results}"
printf 'paper_id,source_path,candidate_binary,binary_exists,recovery_status,notes\n' > "${binary_results}"
printf 'paper_id,source_path,candidate_data,data_exists,data_status,notes\n' > "${data_results}"

csv_quote() { local v="${1//$'\n'/ }"; v="${v//\"/\"\"}"; printf '"%s"' "$v"; }
append_build() {
  local paper_id="$1" source_path="$2" cmd="$3" attempt="$4" exit_code="$5" timeout_flag="$6" binary="$7" status="$8" log="$9" notes="${10}"
  { csv_quote "$paper_id"; printf ','; csv_quote "$source_path"; printf ','; csv_quote "$cmd"; printf ','; csv_quote "$attempt"; printf ','; csv_quote "$exit_code"; printf ','; csv_quote "$timeout_flag"; printf ','; csv_quote "$binary"; printf ','; csv_quote "$status"; printf ','; csv_quote "$log"; printf ','; csv_quote "$notes"; printf '\n'; } >> "${build_results}"
}
append_binary() {
  local paper_id="$1" source_path="$2" binary="$3" exists="$4" status="$5" notes="$6"
  { csv_quote "$paper_id"; printf ','; csv_quote "$source_path"; printf ','; csv_quote "$binary"; printf ','; csv_quote "$exists"; printf ','; csv_quote "$status"; printf ','; csv_quote "$notes"; printf '\n'; } >> "${binary_results}"
}
append_data() {
  local paper_id="$1" source_path="$2" data="$3" exists="$4" status="$5" notes="$6"
  { csv_quote "$paper_id"; printf ','; csv_quote "$source_path"; printf ','; csv_quote "$data"; printf ','; csv_quote "$exists"; printf ','; csv_quote "$status"; printf ','; csv_quote "$notes"; printf '\n'; } >> "${data_results}"
}
find_binary() {
  local source_path="$1"
  local candidate
  while IFS= read -r candidate; do
    if file -b "$candidate" 2>/dev/null | grep -q '^ELF'; then
      printf '%s\n' "$candidate"
      return 0
    fi
  done < <(
    find "$source_path" -maxdepth 5 -type f -perm -111 \
      ! -name '*.sh' ! -name '*.py' ! -name '*.pl' ! -name '*.c' ! -name '*.cu' \
      ! -name '*.cc' ! -name '*.cpp' ! -name '*.h' ! -name '*.hpp' ! -name '*.txt' \
      ! -name '*.dat' ! -name '*.fa' ! -name '*.fna' ! -name '*.stats' ! -name '*.md5' \
      ! -name '*.xml' ! -name '*.in' ! -name '*.gz' ! -name '*.zip' \
      ! -name 'Makefile' ! -name 'makefile' ! -iname 'README' ! -iname 'COPYING' \
      ! -path '*/tools/*' ! -path '*/meschach_lib/*' 2>/dev/null | sort
  )
}
find_data() {
  local source_path="$1"
  find "$source_path" -maxdepth 5 -type d \( -iname data -o -iname input -o -iname inputs -o -iname datasets \) 2>/dev/null | sort | head -n 1
}
fallback_command() {
  local source_path="$1" native="$2"
  if [[ "$source_path" == *'/parboil/benchmarks/'* ]]; then
    if [[ -f "${source_path}/src/cuda_base/Makefile" ]]; then echo "make -C ${source_path}/src/cuda_base"; return; fi
    if [[ -f "${source_path}/src/cuda-base/Makefile" ]]; then echo "make -C ${source_path}/src/cuda-base"; return; fi
    if [[ -f "${source_path}/src/base/Makefile" ]]; then echo "make -C ${source_path}/src/base"; return; fi
  fi
  if [[ -f "${source_path}/CUDA/Makefile" ]]; then echo "make -C ${source_path}/CUDA"; return; fi
  if [[ -f "${source_path}/src/Makefile" ]]; then echo "make -C ${source_path}/src"; return; fi
  if [[ -f "${source_path}/makefile" ]]; then echo "make -C ${source_path} -f makefile"; return; fi
  echo "$native"
}
status_from_log() {
  local exit_code="$1" log="$2" timeout_flag="$3"
  if [[ "$timeout_flag" == "1" ]]; then echo failed_timeout; return; fi
  if [[ "$exit_code" == "0" ]]; then echo built; return; fi
  if grep -qiE 'unsupported gpu architecture|compute_[0-9]+|sm_[0-9]+|nvcc fatal|CUDA.*unsupported' "$log" 2>/dev/null; then echo failed_unsupported_cuda; return; fi
  if grep -qiE 'No such file|not found|cannot find|missing|No rule to make target' "$log" 2>/dev/null; then echo failed_missing_dependency; return; fi
  echo failed_compile
}
run_attempt() {
  local paper_id="$1" source_path="$2" cmd="$3" attempt="$4"
  local safe_id="${paper_id}_${attempt//[^A-Za-z0-9_]/_}"
  local log="${W17_LOG_DIR}/${safe_id}.log"
  if [[ "$W17_DRY_RUN" == "1" ]]; then
    append_build "$paper_id" "$source_path" "$cmd" "$attempt" "" "0" "" "dry_run" "$log" "would run build command"
    return 0
  fi
  set +e
  timeout "$W17_BUILD_TIMEOUT_SEC" bash -lc "$cmd" > "$log" 2>&1
  local ec=$?
  set -e
  local timeout_flag=0
  if [[ "$ec" == "124" || "$ec" == "137" ]]; then timeout_flag=1; fi
  local binary=""
  binary="$(find_binary "$source_path" || true)"
  local status
  status="$(status_from_log "$ec" "$log" "$timeout_flag")"
  if [[ -n "$binary" && "$status" != "built" ]]; then status="already_built"; fi
  append_build "$paper_id" "$source_path" "$cmd" "$attempt" "$ec" "$timeout_flag" "$binary" "$status" "$log" ""
}

mapfile -t rows < <(python3 - <<PY
import csv
from pathlib import Path
for r in csv.DictReader(open(${gap_csv@Q})):
    if r['primary_gap'] != 'missing_binary':
        continue
    if ${W17_ONLY_PAPER_ID@Q} and r['paper_id'] != ${W17_ONLY_PAPER_ID@Q}:
        continue
    print('|'.join([r['paper_id'], r['candidate_source'], r['candidate_build_command']]))
PY
)

count=0
for row in "${rows[@]}"; do
  IFS='|' read -r paper_id source_path native_cmd <<< "$row"
  [[ -n "$source_path" ]] || { append_build "$paper_id" "$source_path" "" skipped_no_source "" 0 "" skipped_no_source "" "no candidate source"; continue; }
  [[ -d "$source_path" ]] || { append_build "$paper_id" "$source_path" "" skipped_no_source "" 0 "" skipped_no_source "" "candidate source dir missing"; continue; }
  if [[ "$W17_MAX_BUILDS" -gt 0 && "$count" -ge "$W17_MAX_BUILDS" ]]; then
    append_build "$paper_id" "$source_path" "$native_cmd" skipped_max_builds "" 0 "" skipped_no_build_command "" "W17_MAX_BUILDS cap reached"
    continue
  fi
  existing_binary="$(find_binary "$source_path" || true)"
  if [[ -n "$existing_binary" ]]; then
    append_build "$paper_id" "$source_path" "" binary_recovery "0" 0 "$existing_binary" already_built "" "binary already present before build"
  fi
  if [[ -z "$native_cmd" ]]; then
    append_build "$paper_id" "$source_path" "" skipped_no_build_command "" 0 "$existing_binary" skipped_no_build_command "" "no build command candidate"
  else
    run_attempt "$paper_id" "$source_path" "$native_cmd" native
    fallback_cmd="$(fallback_command "$source_path" "$native_cmd")"
    if [[ "$fallback_cmd" != "$native_cmd" ]]; then
      run_attempt "$paper_id" "$source_path" "$fallback_cmd" fallback
    else
      run_attempt "$paper_id" "$source_path" "$native_cmd" fallback_repeat
    fi
  fi
  recovered_binary="$(find_binary "$source_path" || true)"
  if [[ -n "$recovered_binary" ]]; then append_binary "$paper_id" "$source_path" "$recovered_binary" 1 binary_found "post-build or preexisting executable"; else append_binary "$paper_id" "$source_path" "" 0 binary_missing "no executable recovered"; fi
  data_dir="$(find_data "$source_path" || true)"
  if [[ -n "$data_dir" ]]; then append_data "$paper_id" "$source_path" "$data_dir" 1 data_found "local data directory found"; else append_data "$paper_id" "$source_path" "" 0 data_missing "no local data directory found"; fi
  count=$((count + 1))
done

printf 'build_results=%s\n' "$build_results"
printf 'binary_results=%s\n' "$binary_results"
printf 'data_results=%s\n' "$data_results"
printf 'log_dir=%s\n' "$W17_LOG_DIR"
