# Mascar W24C Guidance: Table III Refreshed Analysis and Trend Report

## Stage position

This is W24C.

W24B executed Table III refreshed sweep. W24C analyzes baseline vs M2/M3/M4, mechanism activation, ready coverage, and trend.

## Goal

Produce refreshed Table III results for current ready rows.

This is not paper-exact reproduction. It is a current-branch/current-config refreshed sweep.

## Analysis script

Create:

- experiments/paper-mascar/workloads/matrix/W24/analyze_w24_tableiii_results.py

Inputs:

- W24 latest results CSV
- W24 latest run manifest
- W24 all manifest
- W24 phase mapping used
- W24 config matrix

Outputs:

- experiments/paper-mascar/workloads/results/W24/w24_tableiii_refreshed_results.csv
- experiments/paper-mascar/workloads/results/W24/w24_tableiii_speedup_summary.csv
- experiments/paper-mascar/workloads/results/W24/w24_tableiii_activation_matrix.csv
- experiments/paper-mascar/workloads/results/W24/w24_tableiii_geomean_summary.csv
- experiments/paper-mascar/workloads/results/W24/w24_tableiii_coverage_manifest.csv
- experiments/paper-mascar/workloads/results/W24/w24_tableiii_phase_mapping_used.csv
- experiments/paper-mascar/workloads/results/W24/w24_tableiii_trend_summary.md

## Required calculations

For each paper_id/config:

- result_status
- correctness_status
- cycles
- ipc
- baseline_cycles
- speedup_vs_baseline
- m1_l1sat_active
- m2_active
- m3_active
- m4_active
- m2_mp_cycles
- m2_owner_acquire
- m3_attempt
- m3_hit
- m3_nack
- m4_enqueue_success
- m4_retry_attempt
- m4_retry_hit
- m4_retry_nack
- phase_mapping_confidence_w18
- measurement_scope
- notes

Activation rules:

- M2 active if m2_mp_cycles > 0 or m2_owner_acquire > 0.
- M3 active if m3_attempt > 0 or m3_hit > 0 or m3_nack > 0.
- M4 active if m4_enqueue_success > 0 or m4_retry_attempt > 0.

Speedup:

- baseline_cycles / config_cycles
- only if both cycles are valid and positive
- do not compute for unavailable/timeout/crash rows

Geomean:

- compute all-valid
- memory-type valid
- compute-type valid
- mark insufficient_data if fewer than 2 valid rows in group

Correctness caveat:

- completed_explicit_pass is correctness evidence.
- completed_stats_found or completed_no_explicit_pass is not correctness pass.
- Report separately.

## Coverage manifest

w24_tableiii_coverage_manifest.csv must include all 30 rows.

Columns:

- paper_id
- paper_name
- paper_type
- availability_status
- wrapper_status
- selected_for_w24
- measurement_scope
- phase_mapping_status_w18
- phase_mapping_confidence_w18
- baseline_status
- m2_status
- m3_status
- m4_status
- m2_active
- m3_active
- m4_active
- valid_for_trend
- correctness_status
- next_action

next_action values:

- use_in_report
- needs_exact_phase_mapping
- needs_workload_availability
- needs_wrapper_fix
- needs_input_scaling
- needs_m3_specific_input
- unavailable
- investigate_failure

## Report

Create:

- docs/papers/mascar_w24_tableiii_refreshed_sweep_report.md

Required sections:

1. Executive summary
2. Scope and caveats
3. Configs
4. Workload coverage
5. Phase mapping used
6. Measurement scope caveat
7. Results table
8. Mechanism activation
9. Memory vs compute trend
10. Correctness status caveat
11. Failures/timeouts
12. Comparison to paper expectations
13. What remains before paper-comparable reproduction
14. Recommendations for W25/W26

Mandatory caveats:

- This is current-branch/current-config, not GPGPU-Sim v3.2.2 GTX480.
- Inferred phase mapping is not exact paper phase mapping.
- App-run results are not strict per-kernel results.
- Do not claim paper 34% speedup reproduction.
- Do not claim paper 12% energy saving reproduction.

## W24C postcheck

Create:

- experiments/paper-mascar/workloads/audit/W24/w24c_postcheck.md

Include:

- start/end/elapsed
- analysis input files
- output row counts
- coverage row count
- valid speedup row count
- M2/M3/M4 active counts
- geomean summary
- warnings

## Validation

Run:

- python3 -m py_compile experiments/paper-mascar/workloads/matrix/W24/analyze_w24_tableiii_results.py
- python3 experiments/paper-mascar/workloads/matrix/W24/analyze_w24_tableiii_results.py
- Check coverage manifest has 30 rows.
- Check refreshed results include selected ready rows x configs.
- Check report exists.
- git diff --check

## Stop conditions

Stop only if:
1. W24 actual results are missing and cannot be regenerated.
2. analyzer loses rows.
3. coverage manifest cannot be generated.

Do not stop because trend is weak or M3 is inactive. Report honestly.
