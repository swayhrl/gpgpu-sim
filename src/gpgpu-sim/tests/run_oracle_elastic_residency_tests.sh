#!/usr/bin/env bash
set -euo pipefail

repo_root=$(cd "$(dirname "${BASH_SOURCE[0]}")/../../.." && pwd)
src="$repo_root/src/gpgpu-sim"
out=${TMPDIR:-/tmp}/c16_oracle_elastic_tests
mkdir -p "$out"

${CXX:-g++} -std=c++11 -Wall -Wextra -Werror -I"$src" \
  "$src/oracle_elastic_residency.cc" \
  "$src/tests/test_oracle_elastic_residency.cc" \
  -o "$out/policy_test"
"$out/policy_test"

${CXX:-g++} -std=c++11 -Wall -Wextra -Werror -I"$src" \
  "$src/oracle_elastic_residency.cc" \
  "$src/tests/m1f_static_selector.cc" \
  -o "$out/m1f_static_selector"
echo "M1F_STATIC_SELECTOR_BUILD_PASS $out/m1f_static_selector"

if [[ -z "${CUDA_INSTALL_PATH:-}" || ! -f "$CUDA_INSTALL_PATH/include/vector_types.h" ]]; then
  echo "CUDA_INSTALL_PATH with headers is required for metadata integration test" >&2
  exit 2
fi
${CXX:-g++} -std=c++11 -Wall -Wextra -Wno-unused-parameter -Wno-reorder \
  -I"$src" -I"$repo_root/src" -I"$CUDA_INSTALL_PATH/include" \
  "$src/oracle_elastic_residency.cc" \
  "$src/tests/test_oracle_elastic_line_metadata.cc" \
  -o "$out/line_metadata_test"
"$out/line_metadata_test"

${CXX:-g++} -std=c++11 -O0 -ffunction-sections -fdata-sections \
  -Wl,--gc-sections -Wno-unused-parameter -Wno-reorder \
  -I"$src" -I"$repo_root/src" -I"$CUDA_INSTALL_PATH/include" \
  "$src/oracle_elastic_residency.cc" "$src/gpu-cache.cc" \
  "$src/gpu-misc.cc" "$src/hashing.cc" \
  "$src/tests/test_oracle_elastic_tag_array_config.cc" \
  -o "$out/tag_array_config_test"
"$out/tag_array_config_test"

echo ORACLE_ELASTIC_QWEIGHT_RESIDENCY_TEST_SUITE_PASS
