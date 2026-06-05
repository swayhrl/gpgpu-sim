# Rodinia/Parboil Suite Framework

## Rodinia/Parboil suite inventory

W20 normalized 23 Rodinia rows and 18 Parboil rows.

## Current ready counts

Current ready baseline: 13; by suite: parboil:5;rodinia:8.

## How to run dry-run all

\\generated_rows=41
generated_manifest=/workspace/repos/gpgpu-sim_distribution/experiments/suites/results/W20/run_dryrun_all_20260605_160059/generated_workload_manifest.csv
GPGPU-Sim version 4.2.0 (build gpgpu-sim_git-commit-c63cc49-modified_1.0) configured with AccelWattch.
setup_environment succeeded
outdir=/workspace/repos/gpgpu-sim_distribution/experiments/suites/results/W20/run_dryrun_all_20260605_160059
run_manifest=/workspace/repos/gpgpu-sim_distribution/experiments/suites/results/W20/run_dryrun_all_20260605_160059/run_manifest.csv
suite_outdir=/workspace/repos/gpgpu-sim_distribution/experiments/suites/results/W20/run_dryrun_all_20260605_160059\

## How to run smoke ready

\\generated_rows=13
generated_manifest=/workspace/repos/gpgpu-sim_distribution/experiments/suites/results/W20/run_smoke_ready_20260605_160101/generated_workload_manifest.csv
GPGPU-Sim version 4.2.0 (build gpgpu-sim_git-commit-c63cc49-modified_1.0) configured with AccelWattch.
setup_environment succeeded\

## How to inspect blockers

Open experiments/suites/common/full_suite_blocker_manifest.csv and experiments/suites/results/W20/w20_blocker_summary.csv.

## How to add or fix wrappers

Repair source/binary/data/command first, run dry-run, then smoke. Promote only rows with explicit pass evidence to current_ready.
