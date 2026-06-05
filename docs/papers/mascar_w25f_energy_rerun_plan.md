# Mascar W25F Energy Rerun Plan

## Goal

Rerun the W25 selected subset with W25E runner/collector recovery: 6 workloads x 2 energy configs = 12 actual rows.

## W25 issue summary

W25 completed 12/12 runs but collected zero true power fields because power artifacts were not available to the collector.

## W25E fix summary

W25E added common runner power artifact recovery into `run_dir/power_artifacts/` and bounded recursive collector scanning while preserving W15 M3 diagnostic and W16 power fields.

## Selected workloads

bp_2, srad_1, bp_1, spmv, mri_q, pathfinder

## Config matrix

- `energy_baseline_off`: `configs/hrl-repro/SM7_QV100_mascar_energy_baseline_off`
- `energy_m4_reexec_load`: `configs/hrl-repro/SM7_QV100_mascar_energy_m4_reexec_load_on`

## Preflight checks

Preflight rows: 12. Failing rows: 0.

## Run plan

Run plan rows: 12.

## Expected outputs

W25F-B will generate dry-run/actual results and power artifact checks. W25F-C will compute current-simulator power and derived-energy trend.

## Caveats

This is a current-simulator AccelWattch/power trend rerun, not paper GPUWattch/GTX480 energy reproduction and not a 12% paper energy-saving claim.
