#!/usr/bin/env bash
set -euo pipefail

root=$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)
build=${GPGPUSIM_BUILD:-"$root/build/gcc-11.4.0/cuda-11080/release"}
out=${TMPDIR:-/tmp}/test_ep_l2_m1_mode_switch
set +u
source "$root/setup_environment" >/dev/null
set -u
mapfile -d '' accelwattch_objs < <(
  find "$build/accelwattch" -maxdepth 1 -name '*.o' ! -name main.o -print0)
mapfile -d '' intersim_objs < <(find "$build/intersim2" -name '*.o' -print0)
${CXX:-g++} -std=c++11 -O2 -I"$CUDA_INSTALL_PATH/include" -I"$root/src" -I"$root" \
  "$root/tests/ep_l2/test_descriptor_mshr_integrated.cc" -Wl,--start-group \
  "$build/gpgpu-sim/libgpu_uarch_sim.a" "$build/cuda-sim/libgpgpu_ptx_sim.a" \
  "$build/libcuda/libcuda.a" "$build/libgpgpusim.a" \
  "${accelwattch_objs[@]}" "${intersim_objs[@]}" -Wl,--end-group \
  -lz -lpthread -ldl -lssl -lcrypto -lGL -o "$out"

invalid="$root/tests/ep_l2/m1_unsupported_feature.config"
if "$out" "$root/configs/tested-cfgs/SM7_QV100/gpgpusim.config" \
    "$invalid" "$invalid" "$invalid" "$invalid" >"${out}.log" 2>&1; then
  echo "M1 mode-switch negative test unexpectedly accepted a feature" >&2
  exit 1
fi
grep -q 'M1 supports only static payload policy with all functional mechanism features OFF' \
  "${out}.log"
echo "EP-L2 M1 mode-switch fail-closed regression: PASS"
