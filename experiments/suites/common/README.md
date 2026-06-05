# Full-Suite Common Framework

## Overview
This directory contains the normalized Rodinia/Parboil full-suite framework created in W20.

## Manifests
- `full_suite_manifest.csv`: canonical normalized manifest.
- `full_suite_command_manifest.csv`: runner-facing command manifest.
- `full_suite_ready_manifest.csv`: current smoke-ready baseline.
- `full_suite_blocker_manifest.csv`: retained blocker rows.
- `full_suite_manifest_quality_report.csv`: row-count and quality checks.

## Status taxonomy
Use `suite_status_taxonomy.csv` and `suite_status_taxonomy.md`. Placeholder and blocker rows are dry-run visible but not actual-run by default.

## Runner usage
Dry-run all rows:

```bash
SUITE_MODE=dryrun_all DRY_RUN_ONLY=1 bash experiments/suites/common/run_full_suite_matrix.sh
```

Smoke current ready rows:

```bash
SUITE_MODE=smoke_ready RUN_PLACEHOLDERS=0 TIMEOUT_SEC=1200 bash experiments/suites/common/run_full_suite_matrix.sh
```

## Collector usage

```bash
python3 experiments/suites/common/collect_full_suite_results.py <run_outdir>
```

## Filters
Supported filters are `FILTER_SUITE`, `FILTER_WORKLOAD`, and `FILTER_STATUS`.

## Raw log policy
Archive raw run directories to `/workspace/tmp` and keep only CSV, summary, and status artifacts in the repo.

## Future paper reuse
Create a normalized workload manifest, create a config matrix, dry-run all rows, smoke ready rows, collect, write postcheck, and package a review pack.
