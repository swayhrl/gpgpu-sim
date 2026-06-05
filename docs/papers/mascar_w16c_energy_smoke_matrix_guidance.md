# Mascar W16C Guidance: Energy Smoke Matrix and Data Collection

## Stage position

This is W16C.

W16A audited energy infrastructure. W16B added collector/config support. W16C runs a small energy matrix if energy configs are feasible.

## Goal

Run a bounded current-simulator energy smoke/trend matrix:

- baseline energy config
- M4 energy config

on a small subset of stable workloads.

## Workload selection

Use stable workloads from previous rounds:

Primary:
- spmv
- mri_q
- pathfinder

Optional if already ready and stable:
- bp_2
- srad_1
- srad_2
- bp_1

Do not include unavailable placeholders.

If W15 microbenchmark is useful and stable, it may be included as non-Table-III diagnostic, but mark it separately.

## Configs

Required if energy configs exist:

1. energy_baseline_off
   configs/hrl-repro/SM7_QV100_mascar_energy_baseline_off

2. energy_m4_reexec_load
   configs/hrl-repro/SM7_QV100_mascar_energy_m4_reexec_load_on

Fallback:
- if no energy configs exist, do not run matrix; produce unavailable report.

## W16 matrix files

Create:

- experiments/paper-mascar/energy/W16C/w16_energy_config_matrix.csv
- experiments/paper-mascar/energy/W16C/w16_energy_workload_manifest.csv
- experiments/paper-mascar/energy/W16C/run_w16_energy_matrix.sh

The run script may use common runner:

- experiments/common/gpgpusim_matrix/run_gpgpusim_matrix.sh

Requirements:
- timeout every run, default 1800 seconds
- no placeholders
- keep logs under experiments/paper-mascar/energy/W16C/results/
- preserve run_manifest/results/summary/status_matrix

## Execution

Run:

1. build check:
   source setup_environment release && make -j2

2. dry-run:
   DRY_RUN_ONLY=1 bash experiments/paper-mascar/energy/W16C/run_w16_energy_matrix.sh

3. collect dry-run:
   python3 experiments/common/gpgpusim_matrix/collect_gpgpusim_stats.py <dryrun_outdir>

4. actual:
   bash experiments/paper-mascar/energy/W16C/run_w16_energy_matrix.sh

5. collect actual:
   python3 experiments/common/gpgpusim_matrix/collect_gpgpusim_stats.py <actual_outdir>

If energy fields are absent after actual run:
- keep runtime results
- mark energy_fields_found=0
- write explicit reason
- do not fabricate energy

If actual run fails due wrapper/config issue:
- debug and fix
- rerun affected subset if feasible

If actual run times out:
- classify timeout
- do not overrun beyond W16 bounds unless only one workload remains and cause is clear

## Required outputs

Create:

- experiments/paper-mascar/energy/W16C/results/
- experiments/paper-mascar/energy/W16C/w16_energy_latest_results.csv
- experiments/paper-mascar/energy/W16C/w16_energy_latest_summary.md
- experiments/paper-mascar/energy/W16C/w16_energy_latest_status_matrix.csv
- experiments/paper-mascar/energy/W16C/w16_energy_latest_run_manifest.csv
- experiments/paper-mascar/energy/W16C/w16c_postcheck.md
- docs/papers/mascar_w16c_energy_smoke_matrix.md

## w16c_postcheck.md

Include:
- start_iso
- end_iso
- elapsed_sec
- branch and HEAD
- git status before/after
- energy config status
- build status
- dry-run status
- actual run status
- workloads run
- row counts
- energy fields found count
- missing energy fields
- raw log archive path
- warnings

## Raw logs

Archive large raw logs to:

- /workspace/tmp/mascar_w16_raw_energy_logs_YYYYMMDD_HHMMSS.tar.gz

Do not commit raw runs directories if large.

Keep only:
- results.csv
- run_manifest.csv
- status_matrix.csv
- summary.md
- stable latest CSV/MD files

## Validation

Run:

- git diff --check
- source setup_environment release && make -j2
- bash -n experiments/paper-mascar/energy/W16C/run_w16_energy_matrix.sh
- python3 -m py_compile experiments/common/gpgpusim_matrix/collect_gpgpusim_stats.py
- collector on actual results if run

## Stop conditions

Stop only if:
1. build cannot be restored
2. generated energy configs break simulator startup and cannot be fixed
3. collector corrupts results
4. all selected workloads fail due same unresolved runtime issue

Do not stop because energy fields are absent. Document unavailable.
