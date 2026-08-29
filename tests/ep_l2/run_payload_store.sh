#!/usr/bin/env bash
set -euo pipefail
root=$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd); out=${TMPDIR:-/tmp}/test_ep_l2_payload_store
set +u; source "$root/setup_environment" >/dev/null; set -u
${CXX:-g++} -std=c++11 -O2 -ffunction-sections -fdata-sections -I"$CUDA_INSTALL_PATH/include" -I"$root/src" -I"$root" \
  "$root/tests/ep_l2/test_payload_store.cc" "$root/src/gpgpu-sim/gpu-cache.cc" -Wl,--gc-sections -o "$out"
"$out"
