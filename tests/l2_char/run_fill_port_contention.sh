#!/usr/bin/env bash
set -euo pipefail

root=$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)
build=${GPGPUSIM_BUILD:-"$root/build/gcc-11.4.0/cuda-11080/release"}
out=${TMPDIR:-/tmp}/test_l2char_fill_port_contention
set +u
source "$root/setup_environment" >/dev/null
set -u
cxx=${CXX:-g++}
mapfile -d '' accelwattch_objs < <(
  find "$build/accelwattch" -maxdepth 1 -name '*.o' ! -name main.o -print0)
mapfile -d '' intersim_objs < <(find "$build/intersim2" -name '*.o' -print0)
"$cxx" -std=c++11 -O2 -I"$CUDA_INSTALL_PATH/include" -I"$root/src" -I"$root" \
  "$root/tests/l2_char/test_fill_port_contention.cc" -Wl,--start-group \
  "$build/gpgpu-sim/libgpu_uarch_sim.a" "$build/cuda-sim/libgpgpu_ptx_sim.a" \
  "$build/libcuda/libcuda.a" "$build/libgpgpusim.a" \
  "${accelwattch_objs[@]}" "${intersim_objs[@]}" -Wl,--end-group \
  -lz -lpthread -ldl -lssl -lcrypto -o "$out"
"$out" "$root/configs/tested-cfgs/SM7_QV100/gpgpusim.config" \
  "$root/tests/l2_char/fill_port_contention.config" | tee "$out.log"
grep -q '^L2CHARV1|INVARIANT|slice=0|status=PASS' "$out.log"
grep -Eq 'fill_eligible=[1-9][0-9]*' "$out.log"
grep -Eq 'fill_blocked=[1-9][0-9]*' "$out.log"
