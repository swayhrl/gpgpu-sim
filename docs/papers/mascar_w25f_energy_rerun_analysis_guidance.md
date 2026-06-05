# Mascar W25F Guidance: Energy Rerun Analysis

## Stage position

This is W25F-C.

W25F-B reran the energy matrix. W25F-C analyzes true power fields and derived energy.

## Goal

Produce the corrected current-simulator energy trend from W25F.

## Analysis script

Create:

- experiments/paper-mascar/energy/W25F/matrix/analyze_w25f_energy_rerun.py

Inputs:

- W25F latest actual results
- W25F run manifest
- W25F power artifact check
- W25F workload manifest
- W25F config matrix
- W16 energy stat map if present
- W25E final diagnosis if present

Outputs:

- experiments/paper-mascar/energy/W25F/results/w25f_energy_trend_results.csv
- experiments/paper-mascar/energy/W25F/results/w25f_energy_ratio_summary.csv
- experiments/paper-mascar/energy/W25F/results/w25f_energy_availability_matrix.csv
- experiments/paper-mascar/energy/W25F/results/w25f_energy_trend_summary.md
- docs/papers/mascar_w25f_energy_rerun_trend_report.md
- experiments/paper-mascar/energy/W25F/audit/w25f_c_postcheck.md

## Required calculations

For each workload:

- baseline_status
- m4_status
- baseline_cycles
- m4_cycles
- baseline_ipc
- m4_ipc
- cycle_ratio = m4_cycles / baseline_cycles
- ipc_ratio = m4_ipc / baseline_ipc
- baseline_kernel_avg_power
- m4_kernel_avg_power
- baseline_gpu_tot_avg_power
- m4_gpu_tot_avg_power
- kernel_power_ratio = m4 / baseline
- gpu_total_power_ratio = m4 / baseline
- baseline_derived_energy
- m4_derived_energy
- derived_energy_ratio
- derived_energy_saving
- artifact_status
- correctness_status
- notes

Derived energy method:

- Prefer direct energy if present.
- Else use gpu_tot_avg_power * runtime_seconds.
- runtime_seconds = cycles / clock_frequency_hz if frequency is available.
- Use W16 established frequency if documented.
- If frequency unavailable, do not compute derived energy; still compute power ratio.

If W16 used 1132 MHz, confirm from W16 docs/config before reusing. If confirmed:
- document frequency_source.
- compute derived energy.

If not confirmed:
- mark derived_energy unavailable, but power trend valid.

## Group summary

Create ratio summary:

- all workloads arithmetic mean power ratio
- all workloads geomean derived energy ratio if available
- memory workloads
- compute workloads
- workloads with prior M2/M4 activation
- workloads with explicit pass vs stats-only

## Report requirements

docs/papers/mascar_w25f_energy_rerun_trend_report.md sections:

1. Executive summary
2. W25 issue and W25E fix recap
3. W25F rerun scope
4. Power artifact recovery results
5. Energy/power field availability
6. Baseline vs M4 power table
7. Derived energy method
8. Derived energy trend table if available
9. Group summaries
10. Caveats
11. Relation to paper energy evaluation
12. Recommendations for future energy runs

Mandatory caveats:

- This is current-simulator AccelWattch/power trend.
- This is not paper GPUWattch/GTX480 result.
- Do not claim paper 12% energy saving reproduction.
- Completed_stats_found is not correctness pass.

## Validation

Run:

- python3 -m py_compile experiments/paper-mascar/energy/W25F/matrix/analyze_w25f_energy_rerun.py
- python3 experiments/paper-mascar/energy/W25F/matrix/analyze_w25f_energy_rerun.py
- Check trend results exist.
- Check ratio summary exists.
- Check report exists.
- Check power rows count is reported.
- git diff --check

## Stop conditions

Stop only if:
1. W25F actual results missing
2. analyzer cannot produce outputs
3. all power fields are missing and W25F-B did not explain why

Do not stop because derived energy is neutral or ratio is 1.0. Report it.
