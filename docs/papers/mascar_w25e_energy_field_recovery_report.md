# Mascar W25E Energy Field Recovery Report

## Goal

Diagnose why W16 parsed `kernel_avg_power` / `gpu_tot_avg_power` but W25 integrated energy sweep produced true power fields = 0.

## W16 observed power fields

W16 results contain `kernel_avg_power`, `gpu_tot_avg_power`, `kernel_max_power`, and `gpu_tot_max_power` for stable workloads such as `spmv`, `mri_q`, and `pathfinder`.

## W25 missing power fields

W25 actual results completed 12/12 runs with stats, but power fields were empty. The W25 `spmv` no-artifact reproduction completed 2/2 and still produced power fields 0/2, matching the W25 failure mode.

## Static config comparison

W16 and W25 use the same energy config directories. Both configs enable AccelWattch with `-power_simulation_enabled 1`, `-power_simulation_mode 0`, and an absolute `-accelwattch_xml_file` path. Config/XML was not the primary root cause.

## Log/output discovery

Discovery found W16 power report hits and W25E recovered power artifacts. The material path difference was wrapper/runner behavior: W16 energy wrappers copied `accelwattch_power_report.log` back to `run_dir`, while W25 generic Table III wrappers did not.

## Minimal rerun result

- `no_artifact`: recovered power rows 0/2.
- `artifact_recovery`: recovered power rows 2/2.
- `common_runner_fix`: recovered power rows 2/2.

Recovered `spmv` power fields in the common W25 runner path:

- `energy_baseline_off`: `kernel_avg_power=55.6973`, `gpu_tot_avg_power=55.6973`, `kernel_max_power=55.8834`, `gpu_tot_max_power=55.8834`.
- `energy_m4_reexec_load`: `kernel_avg_power=55.6973`, `gpu_tot_avg_power=55.6973`, `kernel_max_power=55.8834`, `gpu_tot_max_power=55.8834`.

## Fixes applied

1. `experiments/common/gpgpusim_matrix/run_gpgpusim_matrix.sh`: actual runs now copy new power/wattch/XML artifacts from workload working directories into `run_dir/power_artifacts/`.
2. `experiments/common/gpgpusim_matrix/collect_gpgpusim_stats.py`: collector now recursively scans bounded small text/log/power artifacts under `run_dir`.
3. W25E debug runner documents both the failure reproduction and the recovery strategy.

No Mascar M1-M4 mechanism behavior was modified. W15 M3 diagnostic fields and W16 power fields remain present in the collector.

## Whether power fields recovered

Yes. W25E recovered `kernel_avg_power` and `gpu_tot_avg_power` in the W25 common runner path for `spmv × energy_baseline_off / energy_m4_reexec_load`.
