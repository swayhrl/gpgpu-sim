# CLAUDE NOTES (for next maintainer)

## Task intent
Keep Verilog GPU L2 work isolated from unrelated GPGPU-Sim experiment branches.

## Branching policy
- Work on branch: `hrl/gpu-l2-cache-verilog`
- Do not merge directly into `dev`/`hrl/tlb-latency-v0`/`hrl/project-notes` without review.

## Build/test quick commands
```bash
rtl/benchmarks/run_stage_c_regression.sh quick
rtl/benchmarks/run_stage_c_regression.sh full
```

## Useful focused checks
```bash
iverilog -g2012 -Pgpu_l2_cache_renamed.INDEX_HASH_MODE=1 -o /tmp/tb_p0_h1 \
  rtl/benchmarks/tb_gpu_l2_cache_renamed_p0.v \
  rtl/decouple_cache/gpu_l2_cache_renamed.v rtl/decouple_cache/gpu_l2_tag_dir.v \
  rtl/decouple_cache/gpu_l2_data_pool.v rtl/decouple_cache/gpu_l2_freelist.v \
  rtl/decouple_cache/gpu_l2_hazard_table.v && vvp /tmp/tb_p0_h1
```

## Migration to standalone repo (recommended plan)
1. Create new repository (e.g., `gpu-l2-verilog-cache`).
2. Copy these paths: `rtl/classic_cache`, `rtl/decouple_cache`, relevant `rtl/benchmarks/*`, regression workflow/script.
3. Add top-level docs + license + minimal CI.
4. Add semantic versioning and release tags.
