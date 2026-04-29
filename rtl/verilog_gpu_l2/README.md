# Verilog GPU L2 Cache (Stage-C)

This folder documents the Verilog GPU L2 cache work added in this task.

## Scope
- Classic cache implementation in `rtl/classic_cache/`
- Decoupled (renamed) cache implementation in `rtl/decouple_cache/`
- Directed testbenches in `rtl/benchmarks/`
- Regression driver script `rtl/benchmarks/run_stage_c_regression.sh`
- CI workflow `.github/workflows/rtl-regression.yml`

## Implemented Modules
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

## Address-to-Index Hashing
Both classic and decoupled top modules support `INDEX_HASH_MODE`:
- `0`: direct index (default)
- `1`: fermi-like XOR hashing
- `2`: ipoly-like lightweight mixing

## How to Run Simulation
From repo root:

```bash
rtl/benchmarks/run_stage_c_regression.sh quick
rtl/benchmarks/run_stage_c_regression.sh full
```

## Dependencies
- Required: **Icarus Verilog** (`iverilog`, `vvp`)
- Optional: Verilator (not required by current regression script)

## Known Issues / Limitations
- Current `full` benchmark path runs classic cache benchmark; decoupled perf analysis relies on directed testbenches such as `tb_gpu_l2_cache_perf_p34.v`.
- Hash-mode validation beyond directed tests may need additional workload generators.
- RTL is functional for Stage-C style validation but not yet packaged as a standalone reusable IP repo.

