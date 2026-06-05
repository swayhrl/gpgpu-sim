# W16C Postcheck

start_ts=1780629197
end_ts=1780637218
elapsed_sec=8021

## Actual Smoke Matrix

- configs: energy_baseline_off, energy_m4_reexec_load
- workloads: spmv, mri_q, pathfinder
- timeout_sec: 1800 per actual run
- final actual run: experiments/paper-mascar/energy/W16C/results/w16_energy_actual_final

## Result

- rows: 6
- rows_with_power_or_energy_fields: 6
- classifications: 2 completed_explicit_pass, 4 completed_no_explicit_pass
- true power fields found: kernel_avg_power, gpu_tot_avg_power, kernel_max_power, gpu_tot_max_power
- direct energy fields found: none

## Tool/Config Fixes Applied

- Set GPGPUSIM_CONFIG_OVERRIDE in the W16 wrapper so workload runner uses the energy config in the binary exec directory.
- Switched AccelWattch XML config to an absolute path to avoid cwd-dependent XML lookup failures.
- Added marker-based power report copy from workload root to matrix run_dir so collector can parse reports.
