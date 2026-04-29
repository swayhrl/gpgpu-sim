# MIGRATION PLAN — Verilog GPU L2 cache standalone repository

## 1) Proposed target repository layout

```text
gpu-l2-cache-verilog/
  README.md
  HANDOFF.md
  CLAUDE.md
  AGENTS.md
  rtl/
    classic_cache/
    decouple_cache/
  tb/
    *.v
  scripts/
    run_regression.sh
  docs/
    EXPORT_MANIFEST.md
    MIGRATION_PLAN.md
  .github/
    workflows/
      rtl-regression.yml
```

## 2) Why this structure

- Keep **core RTL** in `rtl/` (stable integration boundary for downstream users).
- Separate **verification entry points** into `tb/` for clearer dependency graph and easier CI matrixing.
- Place executable helpers in `scripts/` to avoid mixing shell tooling with RTL/testbench source.
- Move process/handoff docs to top-level and planning docs to `docs/` so maintainers can quickly find onboarding content.

## 3) Old→new path mapping

| Old path (gpgpu-sim) | New path (standalone) | Action |
|---|---|---|
| `rtl/classic_cache/` | `rtl/classic_cache/` | Keep path, copy as-is. |
| `rtl/decouple_cache/` | `rtl/decouple_cache/` | Keep path, copy as-is. |
| `rtl/benchmarks/tb_gpu_l2_*.v` | `tb/tb_gpu_l2_*.v` | Move testbenches under `tb/`. |
| `rtl/benchmarks/run_stage_c_regression.sh` | `scripts/run_regression.sh` | Move + rename script for neutral naming. |
| `.github/workflows/rtl-regression.yml` | `.github/workflows/rtl-regression.yml` | Copy and patch paths. |
| `rtl/verilog_gpu_l2/README.md` | `README.md` | Promote to top-level. |
| `rtl/verilog_gpu_l2/HANDOFF.md` | `HANDOFF.md` | Promote to top-level. |
| `rtl/verilog_gpu_l2/CLAUDE.md` | `CLAUDE.md` | Promote to top-level. |
| `rtl/verilog_gpu_l2/AGENTS.md` | `AGENTS.md` | Promote to top-level. |
| `rtl/verilog_gpu_l2/EXPORT_MANIFEST.md` | `docs/EXPORT_MANIFEST.md` | Move planning doc. |
| `rtl/verilog_gpu_l2/MIGRATION_PLAN.md` | `docs/MIGRATION_PLAN.md` | Move planning doc. |

## 4) Expected renames and reference updates

### Required file renames
- `rtl/benchmarks/run_stage_c_regression.sh` → `scripts/run_regression.sh`

### Required include/path updates
- Regression script compile paths: from `rtl/benchmarks/...` to `tb/...`.
- Workflow steps invoking script: from `rtl/benchmarks/run_stage_c_regression.sh` to `scripts/run_regression.sh`.
- README commands: update all examples to new script and new directory layout.

## 5) CI migration adjustments (`.github/workflows/rtl-regression.yml`)

After migration:
1. Keep Ubuntu runner and `iverilog` install step.
2. Update invocation path to `scripts/run_regression.sh quick` and optionally `full`.
3. If workflow currently checks any gpgpu-sim-specific files, remove those checks.
4. Optional: split quick/full into matrix jobs for clearer pass/fail visibility.

## 6) README command changes after migration

### Current (inside gpgpu-sim fork)
```bash
rtl/benchmarks/run_stage_c_regression.sh quick
rtl/benchmarks/run_stage_c_regression.sh full
```

### Future (standalone repo root)
```bash
scripts/run_regression.sh quick
scripts/run_regression.sh full
```

## 7) Migration guardrails

- Do **not** merge migration prep branch into unrelated long-lived branches during packaging.
- Keep first standalone import as a history-preserving or snapshot baseline, then do cleanup commits separately.
- Avoid RTL functional rewrites during extraction; focus on path/layout determinism first.
