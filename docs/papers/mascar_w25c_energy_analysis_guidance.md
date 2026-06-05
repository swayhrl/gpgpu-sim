# Mascar W25C Guidance: Integrated Energy Analysis

## Stage position

This is W25C.

W25B ran the energy sweep. W25C analyzes current-simulator energy trend.

## Goal

Compute and report baseline vs M4 power/derived-energy trend.

This analysis is current-simulator only. It must not claim paper GPUWattch/GTX480 equivalence.

## Analysis script

Create:

- experiments/paper-mascar/energy/W25/matrix/analyze_w25_energy_results.py

Inputs:
- W25 actual latest results
- W25 run manifest
- W25 workload manifest
- W16 energy stat map
- optional W24 activation results

Outputs:
- experiments/paper-mascar/energy/W25/results/w25_energy_trend_results.csv
- experiments/paper-mascar/energy/W25/results/w25_energy_ratio_summary.csv
- experiments/paper-mascar/energy/W25/results/w25_energy_availability_matrix.csv
- experiments/paper-mascar/energy/W25/results/w25_energy_trend_summary.md

## Required calculations

For each workload:

- baseline_status
- m4_status
- baseline_cycles
- m4_cycles
- cycle_ratio = m4_cycles / baseline_cycles
- baseline_ipc
- m4_ipc
- ipc_ratio
- baseline_kernel_avg_power
- m4_kernel_avg_power
- baseline_gpu_tot_avg_power
- m4_gpu_tot_avg_power
- power_ratio_kernel
- power_ratio_gpu_total
- baseline_derived_energy
- m4_derived_energy
- derived_energy_ratio
- derived_energy_saving
- m2_active_prior
- m3_active_prior
- m4_active_prior
- correctness_status
- notes

Derived energy:

- Prefer direct energy_total if present.
- Else use gpu_tot_avg_power * runtime_seconds if available.
- runtime_seconds can be derived from cycles / core_freq_hz if frequency can be read from config or W16 documentation.
- If no reliable runtime/frequency, mark derived_energy unavailable.
- Do not invent frequency.

If W16 already established a frequency value, reuse it and cite W16 doc in notes.

## Group summaries

Compute:
- all valid workloads average ratio
- memory-type workloads average ratio if enough data
- compute-type workloads average ratio if enough data
- active-prior workloads ratio
- inactive-prior workloads ratio

Use arithmetic mean for power ratios and also geomean for energy ratios if values are positive.

## Report

Create:

- docs/papers/mascar_w25_integrated_energy_sweep_report.md

Required sections:

1. Executive summary
2. Scope and caveat
3. Workloads and configs
4. Energy/power fields found
5. Derived energy method
6. Baseline vs M4 table
7. Group trend summary
8. Relation to W24 performance/activation
9. Differences from paper GPUWattch energy evaluation
10. Limitations
11. Recommendations for future energy runs

Mandatory caveat:
- This is current-simulator energy trend only.
- It is not a paper-equivalent GPUWattch/GTX480 result.
- It does not reproduce the paper's 12% energy saving unless explicitly shown under comparable setup.

## W25C postcheck

Create:

- experiments/paper-mascar/energy/W25/audit/w25c_postcheck.md

Include:
- start/end/elapsed
- analysis input files
- valid workload count
- energy field availability
- derived energy method
- ratio summary
- warnings

## Validation

Run:
- python3 -m py_compile experiments/paper-mascar/energy/W25/matrix/analyze_w25_energy_results.py
- python3 experiments/paper-mascar/energy/W25/matrix/analyze_w25_energy_results.py
- Check output CSVs exist.
- Check report exists.
- git diff --check

## Stop conditions

Stop only if:
1. actual result CSV is missing and cannot be regenerated
2. analyzer cannot produce output
3. collector results are malformed

Do not stop because trend is neutral or no energy saving is observed.
