# Mascar W23 Baseline Characterization Report

## Goal
Characterize current simulator baseline metrics for the W22 smoke-ready workload set.

## Workloads
Baseline characterization workload count: 13.
Metrics parsed: 13.

## Classification
Smoke classifications from W22 baseline_off run: {'completed_stats_found': 11, 'completed_explicit_pass': 2}.
Correctness notes: {'stats_only_not_correctness_pass': 11, 'explicit_pass': 2}.

## Metrics captured
The characterization CSV records `gpu_tot_sim_cycle`, `gpu_tot_sim_insn`, `gpu_tot_ipc`, L1D cache fields, Mascar M4 counters, and energy fields when present.

## Limitations
W23 reports current-simulator baseline characteristics only. It does not claim correctness pass for stats-only completions and does not run M1-M4 mechanism comparisons.
