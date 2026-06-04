# Mascar W8 Activation Trend Report

## Scope

W8 is a focused sweep over the W7 activation-ready candidate subset:

- `spmv`
- `mri_q`
- `pathfinder`

The sweep uses four configs: `baseline_off`, `m2_owner_sched`, `m3_hitonly_nack`, and `m4_reexec_load`. It is not a full Table III sweep and is not comparable to the Mascar paper's reported speedup or energy results.

## Run Plan

W8 keeps the Table III workload manifest at 30 rows and runs only the three W7 focused workloads. Placeholder and unavailable rows remain represented in `w8_workload_manifest.csv`, while `w8_run_plan.csv` contains the 3 workload x 4 config execution matrix.

Generated plan artifacts:

- `experiments/paper-mascar/workloads/matrix/W8/w8_workload_manifest.csv`
- `experiments/paper-mascar/workloads/matrix/W8/w8_command_manifest_subset.csv`
- `experiments/paper-mascar/workloads/matrix/W8/w8_run_plan.csv`
- `experiments/paper-mascar/workloads/matrix/W8/w8_config_matrix.csv`

## Execution

The final actual sweep used:

- run directory: `experiments/paper-mascar/workloads/results/W8/w8_focused_actual_configfix`
- latest results: `experiments/paper-mascar/workloads/results/W8/w8_latest_results.csv`
- latest status matrix: `experiments/paper-mascar/workloads/results/W8/w8_latest_status_matrix.csv`
- latest run manifest: `experiments/paper-mascar/workloads/results/W8/w8_latest_run_manifest.csv`

Run status:

- planned rows: 12
- result rows: 12
- missing planned pairs: 0
- completed explicit pass: 4
- completed with stats found: 8
- timeout/crash/unavailable rows in focused run: 0

`spmv` reports explicit pass under all four configs. `mri_q` and `pathfinder` completed with GPGPU-Sim stats but do not provide explicit pass markers in this wrapper path, so they are not counted as correctness passes.

## Activation

Focused workload activation counts:

- M2 active workloads: 3 (`spmv`, `mri_q`, `pathfinder`)
- M3 active workloads: 0
- M4 active workloads: 3 (`spmv`, `mri_q`, `pathfinder`)
- any M2/M3/M4 active: 3

M2 activity is identified from owner/MP counters. M4 activity is identified from re-exec queue counters. M3 hit-only counters remain zero for this focused subset.

## Preliminary Trend

Cycle ratios are computed against `baseline_off` for the same workload. For this W8 focused run, `gpu_tot_sim_cycle` is identical across baseline/M2/M3/M4 for all three workloads, so the cycle-speedup geomean is `1.000000` for each active config.

This should be interpreted as "mechanism counters activate without a measured cycle trend in this focused smoke/full-sweep subset", not as a paper-level performance result.

## Debug Note

The first W8 actual attempt exposed a wrapper/config bug: direct Table III wrappers execute binaries in workload directories where stale `gpgpusim.config` files can override the intended matrix config. The common wrapper now temporarily installs the requested `MASCAR_CONFIG_DIR` files into the workload CWD before actual execution and restores the original files after the run.

The fixed run confirms `baseline_off` logs `paper_mascar_enabled = 0` and zero M1-M4 counters. No M1-M4 Mascar mechanism code was changed.

## Limitations

- This is a three-workload focused sweep, not the full Table III coverage.
- `completed_stats_found` is not treated as an explicit correctness pass.
- M3 hit-only activation was not observed under these inputs.
- Energy behavior was not evaluated.
- Results do not claim reproduction of the paper's 34% speedup or 12% energy saving.
