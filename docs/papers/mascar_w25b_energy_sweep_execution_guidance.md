# Mascar W25B Guidance: Integrated Energy Sweep Execution

## Stage position

This is W25B.

W25A prepared the run plan. W25B executes current-simulator energy sweep.

## Goal

Run a bounded energy matrix:

- selected workloads
- energy_baseline_off
- energy_m4_reexec_load

Collect power/energy fields using the common collector.

## Hard constraints

1. Do not run full benchmark suites.
2. Do not actual-run unavailable rows.
3. Every actual run must use timeout.
4. Continue after individual failures.
5. If energy fields disappear, record unavailable; do not fabricate fields.
6. If wrapper/config issue is found, fix locally and rerun affected subset if feasible.
7. Do not modify Mascar M1-M4 behavior.
8. Raw logs must be archived to /workspace/tmp if large.
9. Do not commit W25 outputs.

## Runner script

Create:

- experiments/paper-mascar/energy/W25/matrix/run_w25_energy_sweep.sh

It may call:

- experiments/common/gpgpusim_matrix/run_gpgpusim_matrix.sh

Environment variables:

- W25_CONFIG_MATRIX
- W25_WORKLOAD_MANIFEST
- W25_OUTDIR
- W25_TIMEOUT_SEC default 1800
- W25_MAX_RUNS default 0
- W25_DRY_RUN_ONLY default 0
- W25_FILTER_WORKLOAD optional
- W25_FILTER_CONFIG optional

Default:
- W25_CONFIG_MATRIX=experiments/paper-mascar/energy/W25/matrix/w25_energy_config_matrix.csv
- W25_WORKLOAD_MANIFEST=experiments/paper-mascar/energy/W25/matrix/w25_energy_workload_manifest.csv
- W25_OUTDIR=experiments/paper-mascar/energy/W25/results/w25_energy_<timestamp>

If the common runner expects W2-style workload manifest, generate:
- experiments/paper-mascar/energy/W25/matrix/w25_energy_command_manifest.csv

## Execution sequence

1. Static checks:
   git diff --check
   bash -n experiments/paper-mascar/energy/W25/matrix/run_w25_energy_sweep.sh
   python3 -m py_compile experiments/common/gpgpusim_matrix/collect_gpgpusim_stats.py

2. Build check:
   source setup_environment release && make -j2

3. Dry-run:
   W25_DRY_RUN_ONLY=1 bash experiments/paper-mascar/energy/W25/matrix/run_w25_energy_sweep.sh

4. Collect dry-run:
   python3 experiments/common/gpgpusim_matrix/collect_gpgpusim_stats.py <dryrun_outdir>

5. Actual:
   bash experiments/paper-mascar/energy/W25/matrix/run_w25_energy_sweep.sh

6. Collect actual:
   python3 experiments/common/gpgpusim_matrix/collect_gpgpusim_stats.py <actual_outdir>

If actual runs are too long:
- keep completed results
- record timeout/failures
- do not expand workload set

## Stable outputs

Create/copy:

- experiments/paper-mascar/energy/W25/results/w25_energy_latest_results.csv
- experiments/paper-mascar/energy/W25/results/w25_energy_latest_summary.md
- experiments/paper-mascar/energy/W25/results/w25_energy_latest_status_matrix.csv
- experiments/paper-mascar/energy/W25/results/w25_energy_latest_run_manifest.csv
- experiments/paper-mascar/energy/W25/results/w25_energy_dryrun_results.csv
- experiments/paper-mascar/energy/W25/results/w25_energy_actual_results.csv

## W25B postcheck

Create:

- experiments/paper-mascar/energy/W25/audit/w25b_postcheck.md

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
- energy field rows
- raw log archive path
- wrapper/config fixes
- warnings

## Energy-field check

After actual collection, count:

- rows with kernel_avg_power
- rows with gpu_tot_avg_power
- rows with energy_total if present
- rows with derived_energy if collector computes it
- rows missing all energy/power fields

If all rows miss energy/power fields:
- W25C must report energy unavailable, not failure.

## Raw log policy

Archive raw run directories to:

- /workspace/tmp/mascar_w25_energy_raw_runs_YYYYMMDD_HHMMSS.tar.gz

Do not commit large runs directories.

## Stop conditions

Stop only if:
1. all actual runs fail due same global framework issue
2. build cannot be restored
3. collector breaks and cannot be restored
4. generated configs are invalid and cannot be fixed

Do not stop because individual workload fails or energy fields are absent. Report honestly.
