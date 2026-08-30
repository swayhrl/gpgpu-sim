#!/usr/bin/env bash
set -euo pipefail

root=$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)
build=${GPGPUSIM_BUILD:-"$root/build/gcc-11.4.0/cuda-11080/release"}
out=${TMPDIR:-/tmp}/test_ep_l2_descriptor_mshr_integrated
set +u
source "$root/setup_environment" >/dev/null
set -u
mapfile -d '' accelwattch_objs < <(
  find "$build/accelwattch" -maxdepth 1 -name '*.o' ! -name main.o -print0)
mapfile -d '' intersim_objs < <(find "$build/intersim2" -name '*.o' -print0)

${CXX:-g++} -std=c++11 -O2 -I"$CUDA_INSTALL_PATH/include" -I"$root/src" -I"$root" \
  "$root/tests/ep_l2/test_descriptor_mshr_integrated.cc" \
  -Wl,--start-group \
  "$build/gpgpu-sim/libgpu_uarch_sim.a" "$build/cuda-sim/libgpgpu_ptx_sim.a" \
  "$build/libcuda/libcuda.a" "$build/libgpgpusim.a" \
  "${accelwattch_objs[@]}" "${intersim_objs[@]}" \
  -Wl,--end-group -lz -lpthread -ldl -lssl -lcrypto -lGL -o "$out"
"$out" "$root/configs/tested-cfgs/SM7_QV100/gpgpusim.config" \
  "$root/tests/ep_l2/integrated.config" \
  "$root/tests/ep_l2/integrated_exhaustion.config" \
  "$root/tests/ep_l2/integrated_stats_off.config" \
  "$root/tests/ep_l2/integrated_banked.config"
