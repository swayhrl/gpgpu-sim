# Mascar W8 Trend Summary

W8 is a focused activation sweep over the W7 subset: `spmv`, `mri_q`, and `pathfinder`.

## Run Counts

- run plan rows: 12
- result rows: 12
- missing planned pairs: 0

## Status Counts

- completed_explicit_pass: 4
- completed_stats_found: 8

## Active Workload Counts

- M2 active workloads: 3
- M3 active workloads: 0
- M4 active workloads: 3
- Any active workloads: 3

## Preliminary Cycle Geomeans

These are focused smoke/full-sweep ratios against `baseline_off`, not paper-comparable speedup claims.

| config | valid pairs | cycle speedup geomean |
| --- | ---: | ---: |
| m2_owner_sched | 3 | 1.000000 |
| m3_hitonly_nack | 3 | 1.000000 |
| m4_reexec_load | 3 | 1.000000 |

## Caveats

- W8 does not run the full Table III workload set.
- `completed_stats_found` rows are not explicit correctness passes.
- M3 hit-only activation remains separate from M2/M4 activation and may remain zero.
