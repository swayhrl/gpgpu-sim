# Mascar W16 Energy Trend Report

## Scope

W16方案A establishes a current GPGPU-Sim 4.x / AccelWattch energy trend pipeline for baseline_off vs m4_reexec_load. This report does not claim reproduction of the paper GPUWattch/GTX480 absolute energy values or the paper's 12% energy saving.

## Infrastructure Result

- Real AccelWattch power fields were found and parsed: kernel_avg_power, gpu_tot_avg_power, kernel_max_power, gpu_tot_max_power.
- Direct energy fields were not emitted by the current logs.
- W16D therefore computes derived current-simulator energy as average_power * runtime_seconds, with runtime estimated from gpu_tot_sim_cycle / 1132 MHz.

## Smoke Matrix

Final actual run: experiments/paper-mascar/energy/W16C/results/w16_energy_actual_final

- configs: energy_baseline_off, energy_m4_reexec_load
- workloads: spmv, mri_q, pathfinder
- rows: 6
- rows with parsed power fields: 6

## Trend

From experiments/paper-mascar/energy/W16D/w16_energy_ratio_by_workload.csv:

- spmv: M4/baseline derived energy ratio 1.0, average power ratio 1.0, cycle ratio 1.0.
- mri_q: M4/baseline derived energy ratio 1.0, average power ratio 1.0, cycle ratio 1.0.
- pathfinder: M4/baseline derived energy ratio 1.0, average power ratio 1.0, cycle ratio 1.0.

For this tiny smoke set, M4 did not change the current-simulator performance or power trend relative to baseline.

## Tool/Config Fixes

- Collector was extended to parse AccelWattch power fields while preserving W15 paper_mascar_m3diag_* fields.
- New isolated energy configs were created under configs/hrl-repro/.
- W16 wrapper now passes GPGPUSIM_CONFIG_OVERRIDE, uses absolute XML config path, and copies AccelWattch reports from workload exec directories into matrix run directories.

## Raw Logs

Raw logs were packed to: /workspace/tmp/mascar_w16_energy_raw_logs_20260605_132658.tar.gz

## Timing

start_ts=1780629197
end_ts=1780637256
elapsed_sec=8059
