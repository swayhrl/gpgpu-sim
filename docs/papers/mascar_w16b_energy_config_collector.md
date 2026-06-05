# Mascar W16B Energy Config and Collector

## Collector Update

`experiments/common/gpgpusim_matrix/collect_gpgpusim_stats.py` now parses AccelWattch power report fields while preserving all existing W15 `paper_mascar_m3diag_*` fields.

New parsed fields include:

- `kernel_avg_power`, `kernel_max_power`, `kernel_min_power`
- `gpu_tot_avg_power`, `gpu_tot_max_power`, `gpu_tot_min_power`
- normalized `power_total_avg`, `power_peak`, `power_total_min`
- candidate direct energy fields `energy_total`, `energy_dynamic`, `energy_leakage`, `energy_dram`, `energy_l2`
- synthetic availability flag `energy_fields_found`

Direct energy fields were not found during W16A audit, so the W16D trend analysis derives estimated current-simulator energy only when real power fields and runtime cycles are available.

## Configs

Created isolated energy config directories:

- `configs/hrl-repro/SM7_QV100_mascar_energy_baseline_off/`
- `configs/hrl-repro/SM7_QV100_mascar_energy_m4_reexec_load_on/`

Both are copied from their original non-energy configs and append only AccelWattch power-report knobs:

```text
-power_simulation_enabled 1
-power_simulation_mode 0
-accelwattch_xml_file accelwattch_ptx_sim.xml
-power_per_cycle_dump 0
-power_trace_enabled 0
-aggregate_power_stats 0
```

The source Mascar mechanism settings are not changed.

## Caveats

- These configs target current GPGPU-Sim / AccelWattch relative trend.
- They do not reproduce the paper's GPUWattch / GTX480 absolute energy setup.
- W16C wrappers must copy the XML into the workload directory and copy `accelwattch_power_report.log` back into the matrix `run_dir` for collector parsing.

## Config Fix During W16C

The first actual W16C pass proved that setting `GPGPUSIM_CONFIG_OVERRIDE` was necessary for the workload runner, and the second pass reached AccelWattch XML parsing but failed with `XML Parsing error ... File not found`. The energy configs were therefore adjusted to use an absolute `-accelwattch_xml_file` path under the repository's tested QV100 config directory. This is a config/tooling fix only; no Mascar mechanism code was changed.
