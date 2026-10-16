#!/usr/bin/env bash
set -euo pipefail
root=$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)
out=${RO_BUILD_ROOT:?Set isolated RO_BUILD_ROOT}/test_ro_transactions
mkdir -p "$(dirname "$out")"
cuda_root=${CUDA_INSTALL_PATH:-/usr/local/cuda-11.8}
${CXX:-g++} -std=c++11 -O2 -Wall -Wextra -ffunction-sections -fdata-sections \
  -I"$cuda_root/include" -I"$root/src" -I"$root" \
  "$root/tests/ep_l2/test_ro_transactions.cc" \
  "$root/src/gpgpu-sim/gpu-cache.cc" -Wl,--gc-sections -o "$out"
"$out"
