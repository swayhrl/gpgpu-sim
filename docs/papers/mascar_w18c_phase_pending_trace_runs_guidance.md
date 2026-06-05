# Mascar W18C Guidance: Phase-Pending Kernel Trace Runs

## Stage position

This is W18C.

W18B added kernel trace. W18C runs trace configs on phase-pending Table III rows and collects kernel launch name/order.

## Goal

Run kernel trace on W17 phase-pending rows and collect structured kernel_trace.csv outputs.

Primary target rows:
- bp_1
- bp_2
- histo_1
- histo_2
- histo_3
- kmeans_1
- kmeans_2
- srad_1
- srad_2

Secondary:
- all ready rows if runtime is acceptable.

## Hard constraints

1. Use trace baseline config by default.
2. Do not run full benchmark suites.
3. Use timeout for every run.
4. Do not claim phase exactness merely from app-level wrapper.
5. Do not treat missing trace as pass.
6. Debug wrapper/config issues if trace config fails.
7. Archive raw logs under /workspace/tmp if large.
8. Do not commit huge run logs.

## Manifests

Create:

- experiments/paper-mascar/workloads/matrix/W18/w18c_trace_config_matrix.csv
- experiments/paper-mascar/workloads/matrix/W18/w18c_trace_workload_manifest.csv
- experiments/paper-mascar/workloads/matrix/W18/run_w18_kernel_trace_matrix.sh

Config matrix:

1. kernel_trace_baseline_off
   config_path=configs/hrl-repro/SM7_QV100_mascar_kernel_trace_baseline_off
   enabled=1

Optional:
2. kernel_trace_m4_reexec_load
   enabled=0 by default

Workload manifest should include primary phase-pending rows. It may include all ready rows if runtime permits.

## Run script

run_w18_kernel_trace_matrix.sh should use common runner:

- experiments/common/gpgpusim_matrix/run_gpgpusim_matrix.sh

Env vars:
- W18_MODE=phase_pending|all_ready
- W18_TIMEOUT_SEC default 1200
- W18_MAX_RUNS default 0
- W18_OUTDIR default experiments/paper-mascar/workloads/results/W18/w18_trace_<timestamp>

Default:
- W18_MODE=phase_pending
- no placeholders
- timeout each run

## Execution

1. build:
   source setup_environment release && make -j2

2. dry-run:
   DRY_RUN_ONLY=1 bash experiments/paper-mascar/workloads/matrix/W18/run_w18_kernel_trace_matrix.sh

3. actual trace:
   bash experiments/paper-mascar/workloads/matrix/W18/run_w18_kernel_trace_matrix.sh

4. collect standard stats:
   python3 experiments/common/gpgpusim_matrix/collect_gpgpusim_stats.py <actual_outdir>

5. collect kernel trace:
   python3 experiments/common/gpgpusim_matrix/collect_kernel_trace.py <actual_outdir> --output <actual_outdir>/kernel_trace.csv

If collect_kernel_trace.py CLI differs, adapt it but document command.

## Stable outputs

Copy or create:

- experiments/paper-mascar/workloads/results/W18/w18_latest_run_manifest.csv
- experiments/paper-mascar/workloads/results/W18/w18_latest_results.csv
- experiments/paper-mascar/workloads/results/W18/w18_latest_summary.md
- experiments/paper-mascar/workloads/results/W18/w18_latest_kernel_trace.csv

## Kernel trace quality checks

Check:
- each run has at least one paperrepro_kernel_begin line
- launch_index is monotonic within a run
- kernel_name is not all unknown if possible
- grid/block are parsed if available
- duplicate app-level rows have consistent launch sequences

If trace is empty:
- inspect logs
- verify trace config is applied
- verify config override path is correct
- fix wrapper/config override issue if needed
- rerun affected subset

## Documentation

Create:

- docs/papers/mascar_w18c_phase_pending_kernel_trace_runs.md

Required sections:
1. Goal
2. Workloads traced
3. Config used
4. Trace run summary
5. Kernel trace quality
6. Failures and fixes
7. Raw log archive note
8. Input to W18D mapping

## Postcheck

Create:

- experiments/paper-mascar/workloads/audit/W18/w18c_postcheck.md

Include:
- start/end/elapsed
- branch and HEAD
- build status
- dry-run status
- actual trace status
- row counts
- number of trace lines
- number of unique kernel names
- empty trace runs
- raw log archive path
- warnings

## Stop conditions

Stop only if:
1. trace config cannot be applied
2. all actual runs fail
3. kernel trace remains empty after debugging config override
4. build cannot be restored

Do not stop because some kernel names are unknown. Document and continue.
