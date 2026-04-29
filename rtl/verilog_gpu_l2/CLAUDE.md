# CLAUDE.md — continuation guide for standalone Verilog L2 repo

## Mission focus
When continuing this project, prioritize:
1. Standalone repo extraction correctness (paths, CI, reproducibility).
2. Regression stability for quick/full flows.
3. Incremental RTL feature work only after migration baseline is clean.

## Working rules
- Avoid mixing simulator-wide gpgpu-sim changes with standalone L2 cache changes.
- Keep commits scoped (layout-only vs behavior change).
- Do not rewrite large RTL blocks during migration commits unless a path/include break blocks regression.

## Default verification loop
```bash
scripts/run_regression.sh quick
scripts/run_regression.sh full
```

If debugging a specific scenario, run only the corresponding testbench compile/run pair and record exact command in commit notes.

## Branching/PR practice
- Use feature branches per topic (e.g., `feat/hash-mode-stress`, `ci/matrix-full`).
- Prefer Draft PR for larger verification-impacting changes.
- Document known failing cases explicitly in PR description until resolved.

## Documentation discipline
When changing folder layout, update in same commit:
- `README.md` command examples
- workflow path references
- any script hardcoded paths

## Definition of done (for nontrivial change)
- Quick regression passes.
- Full regression passes (or failures are explained + tracked).
- README and migration docs remain accurate.
