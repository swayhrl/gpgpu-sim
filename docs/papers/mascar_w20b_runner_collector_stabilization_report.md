# Mascar W20B Runner and Collector Stabilization Report

## Goal
Create reusable Rodinia/Parboil full-suite runner and collector infrastructure without modifying Mascar mechanisms.

## Runner
`run_full_suite_matrix.sh` converts `full_suite_command_manifest.csv` into a temporary common matrix workload manifest, generates per-row wrappers in the run outdir, then calls `experiments/common/gpgpusim_matrix/run_gpgpusim_matrix.sh`.

## Collector
`collect_full_suite_results.py` delegates raw parsing to `collect_gpgpusim_stats.py`, then adds suite metadata, suite classification, correctness policy, `suite_summary.csv`, `blocker_summary.csv`, and `ready_summary.csv`.

## Preserved fields
Because the common collector is reused, existing Mascar M1-M4 counters, W15 `paper_mascar_m3diag_*` counters, and W16 power/energy fields remain available.

## Config matrix
`full_suite_smoke_config_matrix.csv` enables `baseline_off` only. M4 and energy config rows are documented but disabled by default.

## Correctness policy
`completed_no_explicit_pass` and stats-only simulator completion are not correctness passes. Only explicit pass markers become `suite_correctness_pass=1`.

## Fix made during stabilization
The W20 command manifest and runner were fixed to carry `app_name`; this resolved the W20B sample runner/collector path failure.

## Limitation
Wrapper and build repairs for blocked workloads are deferred to W21.
