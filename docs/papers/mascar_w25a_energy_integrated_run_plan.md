# Mascar W25A Integrated Energy Run Plan

## Goal
Run a bounded current-simulator energy trend sweep comparing energy baseline and M4 re-execution configs.

## Paper energy caveat
This is not GPUWattch GTX480/Fermi paper-equivalent energy reproduction and does not claim the paper 12% energy saving.

## W16 energy pipeline status
W16 found current-simulator power fields and derived energy as average power times runtime from cycles.

## W24 inputs used
Fallback mode: 0. W24 refreshed results, coverage, activation, and speedup files were available and used.

## Selected workloads
bp_2, srad_1, bp_1, spmv, mri_q, pathfinder

## Config matrix
Enabled configs: energy_baseline_off, energy_m4_reexec_load.

## Run row count
12 rows = 6 workloads x 2 configs.

## Risks and assumptions
Energy/power field availability depends on current AccelWattch config output. completed_stats_found is not correctness pass.

## W25B execution plan
Run dry-run, actual sweep with timeout, collect with common collector, then analyze current-simulator trend.
