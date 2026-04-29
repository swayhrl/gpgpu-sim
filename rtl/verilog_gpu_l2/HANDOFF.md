# HANDOFF — Verilog GPU L2 cache migration-prep

## Branch/source context
- Workstream source: Codex task **“Implement GPU L2 cache in Verilog”**.
- Staging location: `swayhrl/gpgpu-sim` branch `hrl/gpu-l2-cache-verilog` (target intent).
- This prep pass focuses on **documentation and migration planning**, not broad RTL redesign.

## Main files and directories
- RTL:
  - `rtl/classic_cache/`
  - `rtl/decouple_cache/`
- Verification:
  - `rtl/benchmarks/tb_gpu_l2_*.v`
  - `rtl/benchmarks/run_stage_c_regression.sh`
- CI:
  - `.github/workflows/rtl-regression.yml`
- Migration docs:
  - `rtl/verilog_gpu_l2/README.md`
  - `rtl/verilog_gpu_l2/EXPORT_MANIFEST.md`
  - `rtl/verilog_gpu_l2/MIGRATION_PLAN.md`
  - `rtl/verilog_gpu_l2/CLAUDE.md`
  - `rtl/verilog_gpu_l2/AGENTS.md`

## Known limitations
- Decoupled path still depends mostly on directed bench coverage (not full workload-level flow).
- Some commands assume current gpgpu-sim path layout and must be rewritten after extraction.
- In this local checkout, only `work` branch/ref is present; comparison against `dev` ref may require fetching missing refs before final branch-level audit.

## Migration recommendations
1. Use `EXPORT_MANIFEST.md` as authoritative include/exclude list.
2. Build standalone repo around `rtl/`, `tb/`, `scripts/`, `docs/`, workflow layout from `MIGRATION_PLAN.md`.
3. Keep first migration commit focused on **path moves/renames only**; run regressions; then do cleanup.
4. Preserve this handoff plus `CLAUDE.md` and `AGENTS.md` at top level in the standalone repo.
