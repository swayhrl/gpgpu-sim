# Mascar W20B Guidance: Full-Suite Runner and Collector Stabilization

## Stage position

This is W20B.

W20A normalized manifests and status taxonomy. W20B stabilizes runner and collector infrastructure for full-suite dry-run and smoke.

## Goal

Create stable full-suite runner/collector wrappers around the existing common GPGPU-Sim matrix framework.

The framework must support:
- all Rodinia/Parboil normalized suite rows
- ready-only actual smoke
- full dry-run including placeholders
- filtering by suite/status/workload
- timeout
- resumable output directories
- robust status classification
- cycles/IPC/Mascar counters/W15 m3diag/W16 power fields if available

## Hard constraints

1. Do not modify Mascar M1-M4 mechanism behavior.
2. Do not remove W15 m3diag parsing.
3. Do not remove W16 power parsing.
4. Do not break W3-W18 common runner compatibility.
5. Do not run full benchmark suites in W20B.
6. Do not actual-run placeholders by default.
7. Do not treat completed_no_explicit_pass as correctness pass.
8. Do not use git add . or git add -A.
9. Do not commit W20 outputs.

## Existing tools to inspect

Read:

- experiments/common/gpgpusim_matrix/run_gpgpusim_matrix.sh
- experiments/common/gpgpusim_matrix/collect_gpgpusim_stats.py
- experiments/common/gpgpusim_matrix/collect_kernel_trace.py
- experiments/paper-mascar/workloads/matrix/run_mascar_table_iii_matrix.sh
- experiments/suites/common/run_suite_wrappers.sh if present
- experiments/suites/common/full_suite_command_manifest.csv

## Runner requirements

Create or update:

- experiments/suites/common/run_full_suite_matrix.sh

It should be a thin but robust wrapper over:

- experiments/common/gpgpusim_matrix/run_gpgpusim_matrix.sh

Environment variables:

- SUITE_COMMAND_MANIFEST
- SUITE_CONFIG_MATRIX
- SUITE_OUTDIR
- SUITE_MODE
- FILTER_SUITE
- FILTER_WORKLOAD
- FILTER_STATUS
- RUN_READY
- RUN_PLACEHOLDERS
- DRY_RUN_ONLY
- MAX_RUNS
- TIMEOUT_SEC
- SOURCE_ENV
- GPGPUSIM_ROOT
- GPGPU_WORKLOAD_ROOT

Default behavior:

- SUITE_MODE=dryrun_all
- RUN_READY=1
- RUN_PLACEHOLDERS=1 for dry-run
- RUN_PLACEHOLDERS=0 for actual
- TIMEOUT_SEC=1200
- OUTDIR=experiments/suites/results/W20/<timestamp>

Supported SUITE_MODE values:

- dryrun_all
- dryrun_ready
- smoke_ready
- smoke_sample
- smoke_by_filter

Behavior:
- dryrun_all: run wrapper --dry-run for all rows.
- dryrun_ready: dry-run ready rows only.
- smoke_ready: actual run ready rows only.
- smoke_sample: actual run up to MAX_RUNS ready rows.
- smoke_by_filter: actual run rows matching filters.

The script must:
- create outdir
- write env snapshot
- write command snapshot
- call common runner
- copy or preserve results
- not delete previous results
- continue after individual run failures

## Config matrix

Create:

- experiments/suites/common/full_suite_smoke_config_matrix.csv

Rows:

1. baseline_off
   config_path=configs/hrl-repro/SM7_QV100_mascar_baseline_off
   config_role=baseline
   enabled=1

2. m4_reexec_load
   config_path=configs/hrl-repro/SM7_QV100_mascar_m4_reexec_load_on
   config_role=mascar_m4
   enabled=0 by default

3. energy_baseline_off
   enabled=0 if W16 energy config exists

Do not enable M4/energy by default for full-suite smoke.

## Collector wrapper

Create:

- experiments/suites/common/collect_full_suite_results.py

Responsibilities:
- call or import common collector
- add suite-level summary by suite/status/app
- preserve all fields from common collector
- generate blocker/ready summary
- generate status matrix

Inputs:
- result output dir

Outputs:
- results.csv
- summary.md
- status_matrix.csv
- suite_summary.csv
- blocker_summary.csv
- ready_summary.csv

If importing common collector is hard, shell out to it and post-process its CSV.

## Status classification

Use and preserve common collector classification:

- completed_explicit_pass
- completed_stats_found
- completed_no_explicit_pass
- completed_nonzero_with_stats
- wrapper_unavailable
- timeout
- crash_assert
- simulator_error_no_stats
- missing_binary
- missing_data
- no_stats_exit0
- unknown

Add suite-level classification but do not overwrite raw classification.

## README update

Create or update:

- experiments/suites/common/README.md

Required sections:
1. Overview
2. Manifests
3. Status taxonomy
4. Runner usage
5. Collector usage
6. Dry-run examples
7. Smoke examples
8. Filters
9. Raw log policy
10. How future papers should reuse this

## W20B report

Create:

- docs/papers/mascar_w20b_runner_collector_stabilization_report.md

Required sections:
1. Goal
2. Runner design
3. Collector design
4. Preserved counters
5. Config matrix
6. Status classification
7. Validation
8. Limitations

## W20B postcheck

Create:

- experiments/suites/audit/W20/w20b_postcheck.md

Include:
- start/end/elapsed
- branch and HEAD
- scripts created
- syntax checks
- dry-run command used
- warnings

## Validation

Run:
- bash -n experiments/suites/common/run_full_suite_matrix.sh
- python3 -m py_compile experiments/suites/common/collect_full_suite_results.py
- python3 -m py_compile experiments/common/gpgpusim_matrix/collect_gpgpusim_stats.py
- DRY_RUN_ONLY=1 SUITE_MODE=dryrun_ready MAX_RUNS=2 bash experiments/suites/common/run_full_suite_matrix.sh
- collect on that tiny dry-run output

Do not run full suite actual in W20B.

## Stop conditions

Stop only if:
1. common runner is broken and cannot be restored.
2. collector cannot parse even dry-run output.
3. W15/W16 fields are lost.
4. repository state becomes unsafe.

Do not stop because some wrappers are placeholders or unavailable.
