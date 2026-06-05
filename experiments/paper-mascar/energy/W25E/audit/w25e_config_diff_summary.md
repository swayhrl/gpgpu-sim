# W25E Config Diff Summary

## Finding

W16 and W25 use the same energy config directories and those configs enable `-power_simulation_enabled 1` with an absolute AccelWattch XML path. The material difference is the workload execution wrapper path: W16 used energy-specific wrappers that copied `accelwattch_power_report.log` back to the run directory; W25 used generic Table III wrappers that did not copy AccelWattch power artifacts.

## Config evidence

- energy_baseline_off: power_enabled=1, mode=0, xml=/workspace/repos/gpgpu-sim_distribution/configs/tested-cfgs/SM7_QV100/accelwattch_ptx_sim.xml
- energy_m4_reexec_load: power_enabled=1, mode=0, xml=/workspace/repos/gpgpu-sim_distribution/configs/tested-cfgs/SM7_QV100/accelwattch_ptx_sim.xml
