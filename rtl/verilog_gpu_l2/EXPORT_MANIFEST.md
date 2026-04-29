# EXPORT MANIFEST — Verilog GPU L2 Cache standalone repo

> Purpose: decide what to export from `swayhrl/gpgpu-sim` branch `hrl/gpu-l2-cache-verilog` into a future standalone repository (example: `swayhrl/gpu-l2-cache-verilog`).

## A. Recommended for export

| Current path in gpgpu-sim | Recommended path in standalone repo | Category | Purpose |
|---|---|---|---|
| `rtl/classic_cache/` | `rtl/classic_cache/` | Verilog RTL | Classic GPU L2 cache pipeline and storage structures. |
| `rtl/decouple_cache/` | `rtl/decouple_cache/` | Verilog RTL | Decoupled/renamed GPU L2 cache pipeline and supporting structures. |
| `rtl/benchmarks/tb_gpu_l2_cache_benchmark.v` | `tb/tb_gpu_l2_cache_benchmark.v` | Testbench/benchmark | Classic cache benchmark-style directed simulation entry. |
| `rtl/benchmarks/tb_gpu_l2_cache_perf_p34.v` | `tb/tb_gpu_l2_cache_perf_p34.v` | Testbench/benchmark | Perf/observability-oriented testbench. |
| `rtl/benchmarks/tb_gpu_l2_cache_renamed_p0.v` | `tb/tb_gpu_l2_cache_renamed_p0.v` | Testbench | Decoupled baseline behavior test. |
| `rtl/benchmarks/tb_gpu_l2_cache_renamed_p23_atomic.v` | `tb/tb_gpu_l2_cache_renamed_p23_atomic.v` | Testbench | Atomic operation semantics test. |
| `rtl/benchmarks/tb_gpu_l2_cache_renamed_p24_atomic_sem.v` | `tb/tb_gpu_l2_cache_renamed_p24_atomic_sem.v` | Testbench | Atomic semantic corner-case validation. |
| `rtl/benchmarks/tb_gpu_l2_cache_renamed_p25_readback.v` | `tb/tb_gpu_l2_cache_renamed_p25_readback.v` | Testbench | Data readback correctness test. |
| `rtl/benchmarks/tb_gpu_l2_cache_renamed_p30_flush.v` | `tb/tb_gpu_l2_cache_renamed_p30_flush.v` | Testbench | Flush behavior validation. |
| `rtl/benchmarks/tb_gpu_l2_cache_renamed_p31_ooo_retire.v` | `tb/tb_gpu_l2_cache_renamed_p31_ooo_retire.v` | Testbench | Out-of-order retire / completion ordering checks. |
| `rtl/benchmarks/tb_gpu_l2_freelist_p22.v` | `tb/tb_gpu_l2_freelist_p22.v` | Testbench | Free-list allocator behavior test. |
| `rtl/benchmarks/tb_gpu_l2_hazard_table_p33.v` | `tb/tb_gpu_l2_hazard_table_p33.v` | Testbench | Hazard table conflict/resolution test. |
| `rtl/benchmarks/tb_gpu_l2_tag_dir_p32_sync.v` | `tb/tb_gpu_l2_tag_dir_p32_sync.v` | Testbench | Tag directory synchronization checks. |
| `rtl/benchmarks/run_stage_c_regression.sh` | `scripts/run_regression.sh` | Regression script | Main quick/full regression entry for Icarus Verilog flow. |
| `.github/workflows/rtl-regression.yml` | `.github/workflows/rtl-regression.yml` | CI | GitHub Actions automation for regression runs. |
| `rtl/verilog_gpu_l2/README.md` | `README.md` | Doc | Top-level project overview and usage. |
| `rtl/verilog_gpu_l2/HANDOFF.md` | `HANDOFF.md` | Doc | Handoff status, constraints, and known gaps. |
| `rtl/verilog_gpu_l2/CLAUDE.md` | `CLAUDE.md` | Doc | Claude Code continuation guide. |
| `rtl/verilog_gpu_l2/AGENTS.md` | `AGENTS.md` | Doc | Codex/agent execution conventions for the standalone repo. |
| `rtl/verilog_gpu_l2/EXPORT_MANIFEST.md` | `docs/EXPORT_MANIFEST.md` | Doc | Export inventory and include/exclude rationale. |
| `rtl/verilog_gpu_l2/MIGRATION_PLAN.md` | `docs/MIGRATION_PLAN.md` | Doc | Target structure and migration execution plan. |

## B. Not recommended for export (or only by explicit dependency review)

| Current path in gpgpu-sim | Reason to exclude from initial standalone repo |
|---|---|
| `rtl/benchmarks/` (entire folder wholesale) | Contains only relevant files now, but future gpgpu-sim-local bench files may be mixed in; prefer explicit file list to keep repo focused. |
| Non-RTL simulator trees (`src/`, `libcuda/`, `libopencl/`, `debug_tools/`, build cmake/make scaffolding) | Belong to full GPGPU-Sim simulator distribution, not L2 Verilog IP deliverable. |
| Any branch-management notes outside `rtl/verilog_gpu_l2/` | Usually repository-process metadata, not part of reusable RTL package. |

## C. Directory-specific decision summary

- `rtl/classic_cache/`: **include**; core RTL deliverable.
- `rtl/decouple_cache/`: **include**; core RTL deliverable.
- `rtl/benchmarks/`: **partial include**; include only L2-specific testbenches + regression driver listed above.
- `rtl/verilog_gpu_l2/`: **include selected docs** and relocate to top-level `README/HANDOFF/CLAUDE/AGENTS`, plus `docs/` planning artifacts.
- `.github/workflows/rtl-regression.yml`: **include** with path updates after file relocation.
