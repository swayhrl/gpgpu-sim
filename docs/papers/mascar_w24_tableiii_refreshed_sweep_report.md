# Mascar W24 Table III Refreshed Sweep Report

## Executive summary
W24 ran the current ready Table III rows across baseline, M2, M3, and M4 configs. This is current-branch/current-config evidence, not paper-exact reproduction.

## Scope and caveats
Only ready rows were actual-run. All 30 Table III rows remain represented in coverage. Inferred W18 launch-order mapping is not exact phase mapping. App-run results are not strict per-kernel results.

## Configs
Enabled configs: baseline_off, m2_owner_sched, m3_hitonly_nack, m4_reexec_load.

## Workload coverage
Coverage rows: 30. Selected ready rows: 15. Unavailable rows: 15.

## Phase mapping used
W18 mapping was used without overwriting canonical manifests. `inferred_order` remains inferred.

## Measurement scope caveat
Rows marked `app_run_inferred_phase` are app-level measurements associated with inferred launch order, not strict per-paper kernel timing.

## Results table
See `w24_tableiii_refreshed_results.csv` and `w24_tableiii_speedup_summary.csv`.

## Mechanism activation
M2 active rows: 4. M3 active rows: 0. M4 active rows: 4.

## Memory vs compute trend
See `w24_tableiii_geomean_summary.csv`; geomeans are computed only on valid rows with baseline and config cycles.

## Correctness status caveat
`completed_explicit_pass` is explicit correctness evidence. `completed_no_explicit_pass` / `completed_stats_found` are simulator stats evidence only.

## Failures/timeouts
No timeout rows were observed in the W24 actual sweep. Any unavailable rows are workload availability gaps, not actual-run failures.

## Comparison to paper expectations
This report does not claim Mascar paper 34% speedup or 12% energy saving reproduction. The simulator and GPU target differ from the paper environment.

## What remains before paper-comparable reproduction
Exact phase-level stats, paper-era GPGPU-Sim/GPU configuration alignment, and unavailable workload recovery remain open.

## Recommendations for W25/W26
Add per-kernel stats deltas for inferred phases, create M3-specific input coverage, and continue workload availability repair for unavailable Table III rows.
