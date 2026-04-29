# HANDOFF: Verilog GPU L2 Cache Task

## What was delivered
1. Classic and decoupled Verilog GPU L2 cache implementations.
2. Directed testbench suite for read/write/atomic/flush/hazard/perf scenarios.
3. Quick/full regression script for `iverilog`.
4. CI workflow for automatic regression runs.
5. Additional Stage-D style improvements (hazard structure, observability counters, hash indexing options).

## Where to start reading
1. `rtl/decouple_cache/gpu_l2_cache_renamed.v`
2. `rtl/classic_cache/gpu_l2_cache.v`
3. `rtl/benchmarks/run_stage_c_regression.sh`
4. `rtl/benchmarks/tb_gpu_l2_cache_perf_p34.v`
5. `.github/workflows/rtl-regression.yml`

## Repro steps
```bash
rtl/benchmarks/run_stage_c_regression.sh quick
rtl/benchmarks/run_stage_c_regression.sh full
```

## Suggested next tasks
- Add hash-conflict stress benchmark comparing `INDEX_HASH_MODE=0/1/2`.
- Add decoupled full-system style benchmark path in `full` mode.
- Add packaging boundary (module wrappers + config profiles).
