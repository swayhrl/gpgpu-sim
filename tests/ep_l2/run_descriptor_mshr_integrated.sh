#!/usr/bin/env bash
set -euo pipefail

root=$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)
build=${GPGPUSIM_BUILD:-"$root/build"}
out=${TMPDIR:-/tmp}/test_ep_l2_descriptor_mshr_integrated
set +u
source "$root/setup_environment" >/dev/null
set -u

${CXX:-g++} -std=c++11 -O2 -I"$CUDA_INSTALL_PATH/include" -I"$root/src" -I"$root" \
  "$root/tests/ep_l2/test_descriptor_mshr_integrated.cc" \
  -Wl,--start-group \
  "$build/libentrypoint.a" "$build/libcuda/libcuda.a" \
  "$build/src/cuda-sim/libptxsim.a" "$build/src/gpgpu-sim/libgpgpusim.a" \
  "$build/src/intersim2/libintersim.a" "$build/src/accelwattch/libaccelwattch.a" \
  -Wl,--end-group -lz -lpthread -ldl -lssl -lcrypto -lGL -o "$out"
"$out" "$root/configs/tested-cfgs/SM7_QV100/gpgpusim.config" \
  "$root/tests/ep_l2/integrated.config" \
  "$root/tests/ep_l2/integrated_exhaustion.config" \
  "$root/tests/ep_l2/integrated_stats_off.config" \
  "$root/tests/ep_l2/integrated_banked.config"
