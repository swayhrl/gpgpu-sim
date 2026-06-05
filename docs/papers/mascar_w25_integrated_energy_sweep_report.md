# Mascar W25 Integrated Energy Sweep Report

## Executive summary
W25 ran a bounded current-simulator energy sweep on 6 workloads and 2 energy configs. All actual runs completed with stats, but no power/energy fields were parsed, so energy trend is unavailable for W25.

## Scope and caveat
This is current-simulator energy infrastructure validation, not paper GPUWattch GTX480 energy reproduction and not a claim of 12% paper energy saving.

## Workloads and configs
Workloads: bp_2, srad_1, bp_1, spmv, mri_q, pathfinder. Configs: energy_baseline_off and energy_m4_reexec_load.

## Energy/power fields found
Power/energy field rows: 0 / 6 workloads.

## Derived energy method
W16 uses avg power x runtime from cycles when power fields exist. W25 did not derive energy because current results had no reliable power fields.

## Baseline vs M4 table
See `w25_energy_trend_results.csv`.

## Group trend summary
See `w25_energy_ratio_summary.csv`; energy ratio groups are marked unavailable.

## Relation to W24 performance/activation
Selection prioritized W24 M2/M4-active rows and W16 stable energy workloads. W25 preserves W24 activation history in the trend CSV.

## Differences from paper GPUWattch energy evaluation
Current configs and simulator environment differ from GPGPU-Sim v3.2.2 GTX480/Fermi GPUWattch.

## Limitations
No current W25 power fields were emitted or parsed, so only cycle/ipc context is available. completed_stats_found/completed_no_explicit_pass are not correctness pass.

## Recommendations for future energy runs
Restore/verify AccelWattch power field emission for energy configs, then rerun the same bounded workload set.
