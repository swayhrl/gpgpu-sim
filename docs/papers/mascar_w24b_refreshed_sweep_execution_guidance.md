# Mascar W24B Guidance: Refreshed Table III Sweep Execution

## Stage position

This is W24B.

W24A creates a Table III refreshed sweep run plan. W24B runs the selected ready rows under baseline, M2, M3, and M4 configs.

## Goal

Execute a bounded refreshed Table III sweep for current ready rows and collect logs/results.

## Hard constraints

1. Do not actual-run unavailable rows.
2. Do not run full Rodinia/Parboil suite.
3. Do not modify Mascar M1-M4 mechanism behavior.
4. Every actual run must use timeout.
5. Continue after individual failures.
6. If baseline works but M4 crashes, investigate as potential regression.
7. If both baseline and all Mascar configs fail similarly, classify as workload/wrapper issue.
8. Do not call stats-only rows correctness pass.
9. Archive raw logs under /workspace/tmp if large.
10. Do not commit W24 outputs.

## Runner script

Create:

- experiments/paper-mascar/workloads/matrix/W24/run_w24_tableiii_refreshed_sweep.sh

It may call:

- experiments/common/gpgpusim_matrix/run_gpgpusim_matrix.sh

Environment variables:

- W24_CONFIG_MATRIX
- W24_WORKLOAD_MANIFEST
- W24_OUTDIR
- W24_TIMEOUT_SEC default 1800
- W24_MAX_RUNS default 0
- W24_MODE default ready
- W24_DRY_RUN_ONLY default 0
- W24_FILTER_PAPER_ID optional
- W24_FILTER_CONFIG_ID optional

Default:
- use W24/w24_tableiii_manifest_ready.csv
- use W24/w24_tableiii_config_matrix.csv
- output under experiments/paper-mascar/workloads/results/W24/w24_sweep_<timestamp>

If common runner requires W2-style command manifest columns, create a W24 command manifest adapted from ready manifest.

## Execution sequence

1. Build/sanity:
   git diff --check
   source setup_environment release && make -j2 if simulator source changed or if build freshness is uncertain

2. Dry-run:
   W24_DRY_RUN_ONLY=1 bash experiments/paper-mascar/workloads/matrix/W24/run_w24_tableiii_refreshed_sweep.sh

3. Collect dry-run:
   python3 experiments/common/gpgpusim_matrix/collect_gpgpusim_stats.py <dryrun_outdir>

4. Actual sweep:
   bash experiments/paper-mascar/workloads/matrix/W24/run_w24_tableiii_refreshed_sweep.sh

5. Collect actual:
   python3 experiments/common/gpgpusim_matrix/collect_gpgpusim_stats.py <actual_outdir>

6. If failures:
   - fix wrapper/config issue if localized
   - rerun failed subset if runner supports filtering
   - if not, document and continue

## Stable outputs

Create or copy:

- experiments/paper-mascar/workloads/results/W24/w24_latest_results.csv
- experiments/paper-mascar/workloads/results/W24/w24_latest_summary.md
- experiments/paper-mascar/workloads/results/W24/w24_latest_status_matrix.csv
- experiments/paper-mascar/workloads/results/W24/w24_latest_run_manifest.csv
- experiments/paper-mascar/workloads/results/W24/w24_dryrun_results.csv
- experiments/paper-mascar/workloads/results/W24/w24_dryrun_summary.md
- experiments/paper-mascar/workloads/results/W24/w24_actual_results.csv
- experiments/paper-mascar/workloads/results/W24/w24_actual_summary.md

## W24B postcheck

Create:

- experiments/paper-mascar/workloads/audit/W24/w24b_postcheck.md

Include:

- start_iso
- end_iso
- elapsed_sec
- branch and HEAD
- build status
- dry-run status
- actual run status
- selected ready row count
- enabled config count
- actual run row count
- completed rows
- timeout rows
- crash rows
- stats-only rows
- explicit pass rows
- wrapper fixes made
- raw log archive path
- warnings

## Debug expectations

If config override issue appears:
- inspect wrapper common script.
- fix wrapper/config path only.
- do not change Mascar mechanism.

If some app-level wrappers produce duplicate results:
- acceptable; W24C will mark duplicate/measurement_scope.

If actual sweep is long:
- continue unless timeout budget exceeded.
- do not start full-suite unrelated runs.

## Validation

Run:

- bash -n experiments/paper-mascar/workloads/matrix/W24/run_w24_tableiii_refreshed_sweep.sh
- collector on dry-run and actual outdir
- Check stable latest files exist
- git diff --check

## Stop conditions

Stop only if:
1. common runner is unusable and cannot be fixed.
2. all actual runs fail due framework bug.
3. build cannot be restored.
4. W24 exceeds agreed wall-clock budget.

Do not stop because a few workloads fail. Classify and continue.
