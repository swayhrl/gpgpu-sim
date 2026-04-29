#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "$0")/../.." && pwd)"
cd "$ROOT_DIR"
MODE="${1:-full}"

run_tb() {
  local out="$1"
  shift
  echo "[RUN] $out"
  iverilog -g2012 -o "$out" "$@"
  vvp "$out"
}

COMMON_DEC=(
  rtl/decouple_cache/gpu_l2_cache_renamed.v
  rtl/decouple_cache/gpu_l2_tag_dir.v
  rtl/decouple_cache/gpu_l2_data_pool.v
  rtl/decouple_cache/gpu_l2_freelist.v
  rtl/decouple_cache/gpu_l2_hazard_table.v
)

run_tb /tmp/tb_p0 rtl/benchmarks/tb_gpu_l2_cache_renamed_p0.v "${COMMON_DEC[@]}"
run_tb /tmp/tb_p23 rtl/benchmarks/tb_gpu_l2_cache_renamed_p23_atomic.v "${COMMON_DEC[@]}"
run_tb /tmp/tb_p24 rtl/benchmarks/tb_gpu_l2_cache_renamed_p24_atomic_sem.v "${COMMON_DEC[@]}"
run_tb /tmp/tb_p25 rtl/benchmarks/tb_gpu_l2_cache_renamed_p25_readback.v "${COMMON_DEC[@]}"
run_tb /tmp/tb_p30 rtl/benchmarks/tb_gpu_l2_cache_renamed_p30_flush.v "${COMMON_DEC[@]}"
run_tb /tmp/tb_p31 rtl/benchmarks/tb_gpu_l2_cache_renamed_p31_ooo_retire.v "${COMMON_DEC[@]}"
run_tb /tmp/tb_p32 rtl/benchmarks/tb_gpu_l2_tag_dir_p32_sync.v rtl/decouple_cache/gpu_l2_tag_dir.v
run_tb /tmp/tb_p33 rtl/benchmarks/tb_gpu_l2_hazard_table_p33.v rtl/decouple_cache/gpu_l2_hazard_table.v
run_tb /tmp/tb_p34 rtl/benchmarks/tb_gpu_l2_cache_perf_p34.v "${COMMON_DEC[@]}"
run_tb /tmp/tb_fl rtl/benchmarks/tb_gpu_l2_freelist_p22.v rtl/decouple_cache/gpu_l2_freelist.v

if [[ "$MODE" == "full" ]]; then
  run_tb /tmp/tb_bench rtl/benchmarks/tb_gpu_l2_cache_benchmark.v rtl/classic_cache/gpu_l2_cache.v rtl/classic_cache/gpu_l2_tag_array.v rtl/classic_cache/gpu_l2_data_array.v rtl/classic_cache/gpu_l2_atom_unit.v
fi

echo "[PASS] stage-c regression complete (mode=$MODE)"
