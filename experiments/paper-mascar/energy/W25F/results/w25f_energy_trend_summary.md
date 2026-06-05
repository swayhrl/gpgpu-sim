# W25F Energy Trend Summary

actual_rows=12
rows_with_kernel_and_gpu_power=12
frequency_source=configs/hrl-repro/SM7_QV100_mascar_energy_baseline_off/gpgpusim.config -gpgpu_clock_domains 1132.0:1132.0:1132.0:850.0

## Ratio summary

- all: power_ratio_mean=1, derived_energy_geomean=1.00044, mean_saving=-0.000436667
- memory_rows: power_ratio_mean=1, derived_energy_geomean=1.00068, mean_saving=-0.00068
- compute_rows: power_ratio_mean=1, derived_energy_geomean=1.00019, mean_saving=-0.000193333
- explicit_pass: power_ratio_mean=1, derived_energy_geomean=1, mean_saving=0
- stats_only: power_ratio_mean=1, derived_energy_geomean=1.00052, mean_saving=-0.000524

Caveat: current-simulator AccelWattch trend only; not paper GPUWattch/GTX480 energy-saving reproduction.
