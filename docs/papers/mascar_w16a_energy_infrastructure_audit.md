# Mascar W16A Energy Infrastructure Audit

## Scope

W16A audits whether the current GPGPU-Sim 4.x environment can emit power or energy data for a current-simulator trend comparison between `baseline_off` and `m4_reexec_load`. This is not an absolute reproduction of the paper's GPUWattch/GTX480 energy numbers.

## Findings

- The current source registers AccelWattch options including `-power_simulation_enabled`, `-power_simulation_mode`, `-accelwattch_xml_file`, `-power_per_cycle_dump`, `-power_trace_enabled`, and related knobs.
- QV100 AccelWattch XML files are present under `configs/tested-cfgs/SM7_QV100/`, including `accelwattch_ptx_sim.xml` and SASS/HW variants.
- The existing Mascar HRL configs do not enable power simulation by default.
- The power report path is `accelwattch_power_report.log` in the simulator wrapper defaults.
- Source inspection found real power report fields such as `kernel_avg_power`, `gpu_tot_avg_power`, `kernel_max_power`, and `gpu_tot_max_power`.
- No direct total energy field was found in the audited source/log patterns. W16 will therefore only report derived current-simulator energy estimates when average power and runtime cycles are available.

## Implication for W16

The feasible path is to create isolated energy configs that enable AccelWattch, extend the collector to parse real power fields, run a small smoke matrix, and derive relative energy trend as:

```text
estimated_energy_joules = average_power_watts * runtime_seconds
runtime_seconds = gpu_tot_sim_cycle / core_clock_hz
```

This derived metric is suitable for a current-simulator relative trend, not for claiming the paper's 12% absolute energy saving.

## Evidence Files

- `experiments/paper-mascar/energy/W16A/energy_option_grep.txt`
- `experiments/paper-mascar/energy/W16A/energy_config_candidates.txt`
- `experiments/paper-mascar/energy/W16A/energy_log_field_candidates.txt`
- `experiments/paper-mascar/energy/W16A/e1_energy_stat_map.csv`
