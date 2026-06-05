# Rodinia/Parboil Suite Framework

## Inventory
W20 normalized 23 Rodinia rows and 18 Parboil rows.

## Current ready counts
Current ready baseline: 13; by suite: parboil:5;rodinia:8.

## Dry-run all

```bash
SUITE_MODE=dryrun_all DRY_RUN_ONLY=1 bash experiments/suites/common/run_full_suite_matrix.sh
```

## Smoke ready

```bash
SUITE_MODE=smoke_ready RUN_PLACEHOLDERS=0 TIMEOUT_SEC=1200 bash experiments/suites/common/run_full_suite_matrix.sh
```

## Inspect blockers
Open `experiments/suites/common/full_suite_blocker_manifest.csv` and `experiments/suites/results/W20/w20_blocker_summary.csv`.

## Add or fix wrappers
Repair source, binary, data, and command evidence first. Then run dry-run and smoke. Promote only rows with explicit pass evidence to `current_ready=1`.
