# Mascar W25F Energy Rerun Trend Report

## Executive summary

W25F reran the W25 selected subset after W25E runner/collector recovery. Actual rows: 12. Rows with `kernel_avg_power` and `gpu_tot_avg_power`: 12/12.

## W25 issue and W25E fix recap

W25 completed runs but missed power fields. W25E showed power reports were generated outside collector-visible paths and fixed common runner artifact recovery plus recursive collector scanning.

## W25F rerun scope

Workloads: bp_2, srad_1, bp_1, spmv, mri_q, pathfinder. Configs: `energy_baseline_off` and `energy_m4_reexec_load`.

## Power artifact recovery results

All actual rows have power artifact directories and parsed power fields according to `w25f_power_artifact_check.csv` and `w25f_energy_availability_matrix.csv`.

## Energy/power field availability

`kernel_avg_power` and `gpu_tot_avg_power` are available for 12/12 rows.

## Baseline vs M4 power table

See `w25f_energy_trend_results.csv`.

## Derived energy method

Direct energy fields were not used. Derived energy uses `gpu_tot_avg_power * runtime_seconds`; runtime_seconds is `gpu_tot_sim_cycle / 1132e6`, using the QV100 config clock: `configs/hrl-repro/SM7_QV100_mascar_energy_baseline_off/gpgpusim.config -gpgpu_clock_domains 1132.0:1132.0:1132.0:850.0`.

## Derived energy trend table

See `w25f_energy_ratio_summary.csv`. All-workload arithmetic mean GPU total power ratio is 1; all-workload geomean derived energy ratio is 1.00044.

## Caveats

This is a current-simulator AccelWattch/power trend. It is not the paper GPUWattch/GTX480 result and does not claim the paper's 12% energy saving. `completed_stats_found` and `completed_no_explicit_pass` are not correctness passes.

## Recommendations

Use W25E/W25F common runner artifact recovery for future energy sweeps, keep raw logs archived under `/workspace/tmp`, and report current-simulator trends separately from original-paper absolute reproduction.
