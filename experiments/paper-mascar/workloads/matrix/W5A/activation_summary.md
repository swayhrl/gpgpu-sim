# Mascar W5A Activation Counter Summary

Source: `experiments/paper-mascar/workloads/results/w4_smoke_latest_results.csv` and W4 per-run logs.

Table III rows: 30

## Activation Tier Counts

- missing_binary: 14
- missing_source: 1
- phase_unknown: 9
- telemetry_only: 6

## Ready Workload Counter Status

- bfs (M): tier=telemetry_only, m2_mp=0, m3_attempt=0, m4_enqueue=0, m4_retry=0
- spmv (M): tier=telemetry_only, m2_mp=0, m3_attempt=0, m4_enqueue=0, m4_retry=0
- mri_q (C): tier=telemetry_only, m2_mp=0, m3_attempt=0, m4_enqueue=0, m4_retry=0
- pathfinder (C): tier=telemetry_only, m2_mp=0, m3_attempt=0, m4_enqueue=0, m4_retry=0
- sgemm (C): tier=telemetry_only, m2_mp=0, m3_attempt=0, m4_enqueue=0, m4_retry=0
- stencil (C): tier=telemetry_only, m2_mp=0, m3_attempt=0, m4_enqueue=0, m4_retry=0

## Interpretation

- Mechanism-active Table III rows: 0
- Rows with L1/M2 telemetry counters only: 6
- `telemetry_only` is not treated as MP owner / M3 hit-only / M4 re-exec activation.
- Placeholder rows are preserved and are not counted active.
