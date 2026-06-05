# Mascar W25F Guidance: Energy Rerun Execution

## Stage position

This is W25F-B.

W25F-A prepared the plan. W25F-B reruns the energy matrix using W25E fixed runner and collector.

## Goal

Run 12 rows:

- 6 workloads
- 2 energy configs

and confirm power artifacts are copied into each run_dir and parsed by collector.

## Hard constraints

1. Do not run full benchmark suites.
2. Do not actual-run unavailable rows.
3. Every actual run must use timeout.
4. Do not modify Mascar M1-M4 mechanism behavior.
5. If a run lacks power fields, inspect power_artifacts before declaring failure.
6. If common runner fails to copy artifacts, fix runner locally and rerun affected subset.
7. If collector misses fields in power_artifacts, fix collector and rerun collector first.
8. Raw logs must be archived under /workspace/tmp.
9. Do not commit raw runs directories if large.

## Runner script

Create:

- experiments/paper-mascar/energy/W25F/matrix/run_w25f_energy_rerun.sh

It should call:

- experiments/common/gpgpusim_matrix/run_gpgpusim_matrix.sh

Environment variables:

- W25F_CONFIG_MATRIX
- W25F_WORKLOAD_MANIFEST
- W25F_OUTDIR
- W25F_TIMEOUT_SEC default 1800
- W25F_DRY_RUN_ONLY default 0
- W25F_MAX_RUNS default 0
- W25F_FILTER_WORKLOAD optional
- W25F_FILTER_CONFIG optional

Defaults:

- W25F_CONFIG_MATRIX=experiments/paper-mascar/energy/W25F/matrix/w25f_energy_config_matrix.csv
- W25F_WORKLOAD_MANIFEST=experiments/paper-mascar/energy/W25F/matrix/w25f_energy_workload_manifest.csv
- W25F_OUTDIR=experiments/paper-mascar/energy/W25F/results/w25f_energy_<timestamp>

If the common runner needs W2-style command manifest:
- create experiments/paper-mascar/energy/W25F/matrix/w25f_energy_command_manifest.csv

## Execution sequence

1. Static checks:
   git diff --check
   bash -n experiments/paper-mascar/energy/W25F/matrix/run_w25f_energy_rerun.sh
   python3 -m py_compile experiments/common/gpgpusim_matrix/collect_gpgpusim_stats.py

2. Build check:
   source setup_environment release && make -j2

3. Dry-run:
   W25F_DRY_RUN_ONLY=1 bash experiments/paper-mascar/energy/W25F/matrix/run_w25f_energy_rerun.sh

4. Collect dry-run:
   python3 experiments/common/gpgpusim_matrix/collect_gpgpusim_stats.py <dryrun_outdir>

5. Actual run:
   bash experiments/paper-mascar/energy/W25F/matrix/run_w25f_energy_rerun.sh

6. Collect actual:
   python3 experiments/common/gpgpusim_matrix/collect_gpgpusim_stats.py <actual_outdir>

7. Artifact verification:
   For each actual run_dir:
   - check power_artifacts exists
   - list power artifacts
   - grep kernel_avg_power / gpu_tot_avg_power
   - record in CSV

## Required outputs

Create stable outputs:

- experiments/paper-mascar/energy/W25F/results/w25f_energy_latest_results.csv
- experiments/paper-mascar/energy/W25F/results/w25f_energy_latest_summary.md
- experiments/paper-mascar/energy/W25F/results/w25f_energy_latest_status_matrix.csv
- experiments/paper-mascar/energy/W25F/results/w25f_energy_latest_run_manifest.csv
- experiments/paper-mascar/energy/W25F/results/w25f_energy_dryrun_results.csv
- experiments/paper-mascar/energy/W25F/results/w25f_energy_actual_results.csv
- experiments/paper-mascar/energy/W25F/results/w25f_power_artifact_check.csv
- docs/papers/mascar_w25f_energy_rerun_execution.md
- experiments/paper-mascar/energy/W25F/audit/w25f_b_postcheck.md

## power artifact check CSV

w25f_power_artifact_check.csv columns:

- run_id
- workload_id
- config_id
- run_dir
- power_artifacts_dir_exists
- artifact_count
- has_accelwattch_power_report
- has_kernel_avg_power
- has_gpu_tot_avg_power
- kernel_avg_power
- gpu_tot_avg_power
- notes

## Report requirements

docs/papers/mascar_w25f_energy_rerun_execution.md sections:

1. Goal
2. Commands run
3. Dry-run status
4. Actual run status
5. Artifact recovery status
6. Power field recovery status
7. Failures and fixes
8. Raw log archive note

## W25F-B postcheck

Include:

- start_iso
- end_iso
- elapsed_sec
- branch and HEAD
- build status
- dry-run status
- actual status
- run rows
- completed rows
- timeout rows
- crash rows
- rows with power_artifacts
- rows with kernel_avg_power
- rows with gpu_tot_avg_power
- raw logs archive path
- warnings

## Debug policy

If power fields remain missing:

Step 1:
- inspect power_artifacts directory
- if report exists but collector misses it, fix collector and rerun collector only

Step 2:
- if no artifact exists, inspect wrapper/runner working dir and artifact copy logic
- fix runner artifact recovery and rerun only affected workload/config if possible

Step 3:
- if config has no power output, compare with W25E working minimal run
- fix config path/XML if localized

Do not stop after first failure. Try at least these two fixes:
- collector rerun/fix
- runner artifact recovery/config path fix

## Stop conditions

Stop only if:
1. build cannot be restored
2. all actual runs fail due global framework issue
3. collector cannot be restored
4. runner cannot launch any selected workload

Do not stop because one workload lacks power fields. Classify and continue.
