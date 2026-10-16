#!/usr/bin/env bash
set -euo pipefail
root=$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)
out=${LANE4_TEST_BUILD:?set an isolated LANE4_TEST_BUILD directory}
mkdir -p "$out"
${CXX:-g++} -std=c++17 -O2 -ffunction-sections -fdata-sections \
  -I"${CUDA_INSTALL_PATH:-/usr/local/cuda-11.8}/include" -I"$root/src" -I"$root" \
  "$root/tests/ep_l2/test_ideal_meta.cc" "$root/src/gpgpu-sim/gpu-cache.cc" \
  -Wl,--gc-sections -o "$out/test_ideal_meta"
"$out/test_ideal_meta"
