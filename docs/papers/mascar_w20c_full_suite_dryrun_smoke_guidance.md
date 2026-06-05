# Mascar W20C Guidance: Full-Suite Dry-Run and Smoke Validation

## Stage position

This is W20C.

W20A normalized manifests. W20B created full-suite runner/collector. W20C validates the framework by running dry-run all rows and actual smoke on current ready rows.

## Goal

Prove the full-suite framework works end-to-end.

W20C is not a performance sweep. It is a framework validation pass.

## Run policy

Required:
1. dry-run all normalized full-suite rows.
2. actual smoke ready rows with baseline config.

Optional:
3. smoke sample with M4 config, disabled by default unless runtime is small.
4. energy sample if W16 energy config exists, disabled by default.

## Hard constraints

1. Do not actual-run placeholders by default.
2. Do not run full multi-config benchmark matrix.
3. Every actual run must use timeout.
4. Continue after individual failures.
5. Do not call completed_no_explicit_pass a correctness pass.
6. If wrapper bug is found, fix and rerun affected dry-run/smoke if feasible.
7. Archive raw logs under /workspace/tmp if large.
8. Do not commit raw runs directories if large.
9. Do not modify Mascar M1-M4 mechanism behavior.

## Required commands

Run:

1. Build sanity:
   git diff --check

2. Script syntax:
   bash -n experiments/suites/common/run_full_suite_matrix.sh
   python3 -m py_compile experiments/suites/common/collect_full_suite_results.py

3. Dry-run all:
   SUITE_MODE=dryrun_all DRY_RUN_ONLY=1 bash experiments/suites/common/run_full_suite_matrix.sh

4. Collect dry-run:
   python3 experiments/suites/common/collect_full_suite_results.py <dryrun_outdir>

5. Actual ready smoke:
   SUITE_MODE=smoke_ready RUN_PLACEHOLDERS=0 TIMEOUT_SEC=1200 bash experiments/suites/common/run_full_suite_matrix.sh

6. Collect actual smoke:
   python3 experiments/suites/common/collect_full_suite_results.py <smoke_outdir>

If actual smoke is too large or expected ready count is high:
- still run at least smoke_sample with MAX_RUNS large enough to cover W19 known ready rows if feasible.
- document if full ready smoke skipped.

## Stable outputs

Create:

- experiments/suites/results/W20/w20_dryrun_results.csv
- experiments/suites/results/W20/w20_dryrun_summary.md
- experiments/suites/results/W20/w20_dryrun_status_matrix.csv
- experiments/suites/results/W20/w20_dryrun_run_manifest.csv

- experiments/suites/results/W20/w20_smoke_results.csv
- experiments/suites/results/W20/w20_smoke_summary.md
- experiments/suites/results/W20/w20_smoke_status_matrix.csv
- experiments/suites/results/W20/w20_smoke_run_manifest.csv

- experiments/suites/results/W20/w20_suite_summary.csv
- experiments/suites/results/W20/w20_blocker_summary.csv
- experiments/suites/results/W20/w20_ready_summary.csv

## W20C report

Create:

- docs/papers/mascar_w20c_full_suite_dryrun_smoke_report.md

Required sections:
1. Goal
2. Dry-run all summary
3. Actual ready smoke summary
4. Status breakdown
5. Ready workloads
6. Failed or unavailable workloads
7. Wrapper fixes made
8. Raw log archive
9. What this validates
10. What this does not validate

## W20C postcheck

Create:

- experiments/suites/audit/W20/w20c_postcheck.md

Include:
- start/end/elapsed
- branch and HEAD
- dry-run outdir
- smoke outdir
- row counts
- ready count
- placeholder count
- completed count
- timeout count
- crash count
- no-explicit-pass count
- raw log archive path
- warnings

## Raw log policy

If result directories contain runs logs larger than a few MB:

- archive to /workspace/tmp/mascar_w20_raw_runs_YYYYMMDD_HHMMSS.tar.gz
- remove runs directories from repo tree
- keep CSV/summary/status/run_manifest

## Validation

Run:
- git diff --check
- ensure stable outputs exist
- ensure dryrun row count matches manifest row count
- ensure smoke row count matches ready row count or documented sample count
- ensure summary/report exist

## Stop conditions

Stop only if:
1. dry-run all cannot run due global runner failure.
2. actual smoke cannot run any ready workload due global framework bug.
3. collector corrupts CSV.
4. repository state becomes unsafe.

Do not stop because individual workload fails. Classify and continue.
