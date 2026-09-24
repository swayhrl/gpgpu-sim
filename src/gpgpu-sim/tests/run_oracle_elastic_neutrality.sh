#!/usr/bin/env bash
set -euo pipefail

repo_root=$(cd "$(dirname "${BASH_SOURCE[0]}")/../../.." && pwd)
src="$repo_root/src/gpgpu-sim"
out=${OUTPUT_DIR:-${TMPDIR:-/tmp}/c16_oracle_elastic_neutrality}
mkdir -p "$out"

${CXX:-g++} -std=c++11 -Wall -Wextra -Werror -I"$src" \
  "$src/oracle_elastic_residency.cc" \
  "$src/tests/oracle_elastic_neutrality_harness.cc" \
  -o "$out/harness"

for mode in baseline feature_off enabled_diag_off enabled_diag_on; do
  "$out/harness" "$mode" > "$out/$mode.txt"
done

cmp "$out/baseline.txt" "$out/feature_off.txt"
grep -v '^diagnostic_events=' "$out/enabled_diag_off.txt" > "$out/enabled_diag_off.functional"
grep -v '^diagnostic_events=' "$out/enabled_diag_on.txt" > "$out/enabled_diag_on.functional"
cmp "$out/enabled_diag_off.functional" "$out/enabled_diag_on.functional"
test "$(sed -n 's/^diagnostic_events=//p' "$out/enabled_diag_off.txt")" = 0
test "$(sed -n 's/^diagnostic_events=//p' "$out/enabled_diag_on.txt")" -gt 0
"$src/tests/verify_oracle_elastic_off_neutrality.py" > "$out/off_path_source_neutrality.json"

echo BASELINE_OFF_NEUTRALITY_PASS
echo DIAGNOSTICS_NEUTRALITY_PASS
