# Mascar W6B Guidance: Execute Activation-Aware Full Sweep

## Stage position

This is W6B.

W6A prepares run plans. W6B executes the selected workload/config matrix.

The goal is to run enough Table III-ready workloads to evaluate mechanism activation and preliminary trends. This is larger than W4 smoke but still not a full paper-equivalent Rodinia/Parboil sweep if many workloads are unavailable.

## Hard constraints

1. Do not run unavailable placeholder workloads by default.
2. Do not run all 30 x all configs if most are placeholders.
3. Do not run full benchmark suites outside selected W6 manifests.
4. Use timeout for every actual run.
5. Debug wrapper/config/runtime issues found during W6B.
6. If baseline works but M4 crashes, investigate as potential Mascar bug.
7. If both baseline and M4 fail similarly, classify as workload/env issue unless logs indicate simulator bug.
8. Keep logs and manifests.
9. Do not claim correctness pass without explicit pass marker.
10. Do not commit W6 outputs.

## Runner setup

Use existing W3 common runner:

- experiments/common/gpgpusim_matrix/run_gpgpusim_matrix.sh
- experiments/common/gpgpusim_matrix/collect_gpgpusim_stats.py

If current runner cannot accept the W6 run plan directly, add a thin W6 wrapper:

- experiments/paper-mascar/workloads/matrix/W6/run_w6_activation_sweep.sh

This wrapper should:
- set CONFIG_MATRIX to W6/w6_config_matrix.csv
- set WORKLOAD_MANIFEST to W6/w6_workload_manifest_ready.csv or activation-ready if non-empty
- set OUTDIR to experiments/paper-mascar/workloads/results/W6/w6_sweep_<timestamp>
- set TIMEOUT_SEC default 1200
- set RUN_PLACEHOLDERS=0
- set SOURCE_ENV=1
- call common runner

Add env knobs:

- W6_MODE=activation_ready|ready|all_ready
- W6_MAX_RUNS default 0
- W6_TIMEOUT_SEC default 1200

Behavior:
- If activation_ready manifest has at least one row, first run W6_MODE=activation_ready.
- Then run W6_MODE=ready if runtime budget allows.
- Do not run placeholders in W6B actual sweep.

## Execution sequence

1. Build/check:
   git diff --check
   source setup_environment release && make -j2

2. Dry-run:
   DRY_RUN_ONLY=1 W6_MODE=ready bash experiments/paper-mascar/workloads/matrix/W6/run_w6_activation_sweep.sh

3. Collect dry-run:
   python3 experiments/common/gpgpusim_matrix/collect_gpgpusim_stats.py <dryrun_outdir>

4. Actual activation-ready run:
   If activation_ready manifest has rows:
     W6_MODE=activation_ready bash experiments/paper-mascar/workloads/matrix/W6/run_w6_activation_sweep.sh
     collect results
   Else:
     record activation_ready_empty and skip to ready run.

5. Actual ready run:
   W6_MODE=ready bash experiments/paper-mascar/workloads/matrix/W6/run_w6_activation_sweep.sh
   collect results

6. If failures occur:
   - fix wrapper/config bugs
   - rerun failed subset if runner supports filtering
   - otherwise rerun full ready matrix only if runtime is acceptable

## Result files

Create stable latest copies:

- experiments/paper-mascar/workloads/results/W6/w6_latest_results.csv
- experiments/paper-mascar/workloads/results/W6/w6_latest_summary.md
- experiments/paper-mascar/workloads/results/W6/w6_latest_status_matrix.csv
- experiments/paper-mascar/workloads/results/W6/w6_latest_run_manifest.csv

If both activation-ready and ready runs occur:
- keep both timestamped directories
- latest files should point to the ready run unless activation-ready is the only run
- create combined files if practical:
  - w6_combined_results.csv
  - w6_combined_summary.md

## Debug guidance

If a ready wrapper fails due missing binary/data:
- update command manifest or W6 report; do not fake.

If wrapper command has wrong working directory:
- fix wrapper.

If result_status is completed_nonzero_with_stats:
- keep stats but mark correctness unproven.

If M2/M3/M4 counters all zero:
- do not mark as bug automatically.
- record activation not triggered.

If M4 active has nonzero queue stats:
- record as mechanism activated.
- compare cycles with baseline, but do not overclaim.

If M4 active deadlocks/times out but baseline completes:
- inspect M4 queue/retry logs and counters.
- if localized bug is found, fix.
- rerun build and the failing row/config.

## W6B postcheck

Create:

- experiments/paper-mascar/workloads/audit/W6/w6b_postcheck.md

Include:
- start_iso
- end_iso
- elapsed_sec
- branch and HEAD
- build status
- dry-run status
- actual run status
- number of selected workloads
- number of runs
- completed/timeout/crash/unavailable counts
- failures and fixes
- output dirs
- warnings

## Stop conditions

Stop only if:
1. build cannot be restored.
2. repeated M4-active-only crash reveals a broad M4 design bug.
3. runner corrupts manifests/results.
4. elapsed time exceeds 180 minutes.

Do not stop because no workload triggers counters.
Do not stop because a wrapper is missing.
Do not stop because a completed row lacks explicit PASS.
