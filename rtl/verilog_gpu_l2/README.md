# Verilog GPU L2 Cache (staging inside `swayhrl/gpgpu-sim`)

This directory tracks the migration-prep state for the Verilog GPU L2 cache work currently hosted in the `swayhrl/gpgpu-sim` fork branch `hrl/gpu-l2-cache-verilog`.

> Current state: **still in gpgpu-sim fork branch**.  
> Planned state: migrate to a standalone repo (candidate: `swayhrl/gpu-l2-cache-verilog`).

## Scope
- Classic cache RTL: `rtl/classic_cache/`
- Decoupled cache RTL: `rtl/decouple_cache/`
- Directed testbenches/regression: `rtl/benchmarks/`
- CI workflow: `.github/workflows/rtl-regression.yml`
- Migration docs: `rtl/verilog_gpu_l2/`

## Implemented modules
### Classic cache
- `gpu_l2_cache.v`
- `gpu_l2_tag_array.v`
- `gpu_l2_data_array.v`
- `gpu_l2_atom_unit.v`

### Decoupled cache
- `gpu_l2_cache_renamed.v`
- `gpu_l2_tag_dir.v`
- `gpu_l2_data_pool.v`
- `gpu_l2_freelist.v`
- `gpu_l2_hazard_table.v`

## Address-to-index hashing
`INDEX_HASH_MODE` is supported in top modules:
- `0`: direct index (default)
- `1`: fermi-like XOR hashing
- `2`: ipoly-like lightweight mixing

## Simulation commands (current repo layout)
From gpgpu-sim repo root:

```bash
rtl/benchmarks/run_stage_c_regression.sh quick
rtl/benchmarks/run_stage_c_regression.sh full
```

## Migration references
- Export decision list: `rtl/verilog_gpu_l2/EXPORT_MANIFEST.md`
- Target standalone layout: `rtl/verilog_gpu_l2/MIGRATION_PLAN.md`

## Known limitations
- `full` currently emphasizes classic benchmark path; decoupled perf still relies on directed benches.
- Packaging/layout work is in progress; standalone-repo path conventions are not yet applied in this branch.
