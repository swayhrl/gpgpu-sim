#!/usr/bin/env bash
set -euo pipefail

repo_root=$(cd "$(dirname "${BASH_SOURCE[0]}")/../../.." && pwd)
src="$repo_root/src/gpgpu-sim"
out=${TMPDIR:-/tmp}/c16_strong_baselines_tests
mkdir -p "$out"

${CXX:-g++} -std=c++11 -Wall -Wextra -Werror -I"$src" \
  "$src/c16_strong_baselines.cc" \
  "$src/tests/test_c16_strong_baselines.cc" \
  -o "$out/policy_test"
"$out/policy_test"

if [[ -z "${CUDA_INSTALL_PATH:-}" || ! -f "$CUDA_INSTALL_PATH/include/vector_types.h" ]]; then
  echo "CUDA_INSTALL_PATH with headers is required for tag-array integration test" >&2
  exit 2
fi
${CXX:-g++} -std=c++11 -O0 -ffunction-sections -fdata-sections \
  -Wl,--gc-sections -Wno-unused-parameter -Wno-reorder \
  -I"$src" -I"$repo_root/src" -I"$CUDA_INSTALL_PATH/include" \
  "$src/oracle_elastic_residency.cc" "$src/c16_strong_baselines.cc" \
  "$src/gpu-cache.cc" "$src/gpu-misc.cc" "$src/hashing.cc" \
  "$src/tests/test_c16_strong_baselines_tag_array.cc" \
  -o "$out/tag_array_test"
"$out/tag_array_test"

echo C16_STRONG_BASELINES_TEST_SUITE_PASS
