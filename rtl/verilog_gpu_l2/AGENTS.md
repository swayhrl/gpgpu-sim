# AGENTS.md — Codex operating instructions (standalone Verilog L2 repo)

## Scope
This file is intended to be moved to the standalone `gpu-l2-cache-verilog` repository root.

## Priorities for future Codex runs
1. Keep RTL (`rtl/`) and testbenches (`tb/`) cleanly separated.
2. Keep scripts in `scripts/` and CI in `.github/workflows/`.
3. Preserve deterministic command-line regression entry points.

## Do / Don't
### Do
- Run regression after any RTL or path change.
- Update README and CI paths in same commit as structural moves.
- Keep commits narrowly scoped and reviewable.

### Don't
- Mix unrelated simulator code into this repo.
- Perform force-push rewrite on shared branches unless explicitly requested by humans.
- Introduce large RTL refactors as part of migration-only tasks.

## Standard checks
```bash
scripts/run_regression.sh quick
scripts/run_regression.sh full
```

## Commit message guidance
Use imperative and scoped titles, e.g.:
- `Prepare standalone repo migration docs`
- `Update CI paths after tb/scripts split`
- `Add directed hazard-table regression case`
