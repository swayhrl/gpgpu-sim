# Mascar W16C Energy Smoke Matrix

## Scope

W16C ran a small current-simulator energy smoke matrix, not the full Table III benchmark suite.

## Matrix

Configs:

- `energy_baseline_off`: baseline-off config with AccelWattch enabled.
- `energy_m4_reexec_load`: M4 re-exec-load config with AccelWattch enabled.

Workloads:

- `spmv`
- `mri_q`
- `pathfinder`

Each actual run used timeout 1800 seconds.

## Fixes Required to Run

- `GPGPUSIM_CONFIG_OVERRIDE` had to be passed to the workload runner so the override config was copied to each benchmark exec directory.
- `-accelwattch_xml_file` was changed to an absolute XML path because relative XML lookup failed under workload cwd.
- AccelWattch wrote `accelwattch_power_report.log` in benchmark exec directories, not matrix run directories. The W16 wrapper now uses a marker-based workload-root scan and copies the report into the corresponding run directory.

## Final Result

The final run `w16_energy_actual_final` completed 6/6 runs and collector parsed real AccelWattch power fields for all rows. Direct energy fields were not emitted; W16D derives current-simulator energy estimates from average power and runtime cycles.
