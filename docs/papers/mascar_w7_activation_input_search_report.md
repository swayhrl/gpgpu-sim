# Mascar W7 Activation Input Search Report

## Goal

W7 strengthens the six ready Table III workload candidate inputs from W6 to find workloads that trigger Mascar active counters. This round does not modify M1-M4 Mascar mechanism code.

## W7A Input Changes

W6 used W5C iter2 bounded inputs. W7 iteration 1 switches to the larger W5C inputs:

| workload | W7 input | intent |
| --- | --- | --- |
| `bfs` | `bfs_graph_1024_d16.txt` | more BFS nodes and edge-list pressure |
| `spmv` | `spmv_1024_d16.mtx` plus vector | more irregular sparse loads |
| `mri_q` | `mriq_k128_x128.bin 128` | larger sanity input |
| `pathfinder` | `256 256 4` | larger active grid |
| `sgemm` | `128x64x128` generated matrices | more coalesced memory and compute work |
| `stencil` | `16x16x16`, two iterations | larger grid memory footprint |

All runs used the existing `m4_reexec_load` config. That config enables M2 owner scheduling, M3 hit-only/NACK, and M4 load re-execution queue behavior.

## W7A Runtime Result

The W7 actual smoke ran six ready workloads with 420-second per-run timeouts.

Status:

- completed rows: 6
- timeouts: 0
- crashes: 0
- `completed_explicit_pass`: 1
- `completed_stats_found`: 5

`spmv` emitted an explicit pass signal. The other five workloads completed with simulator stats, but are not treated as correctness-pass rows.

## W7B Activation-Ready Subset

W7 found an activation-ready subset:

- `spmv`
- `mri_q`
- `pathfinder`

The subset manifest is:

```text
experiments/paper-mascar/workloads/matrix/W7/activation_ready_subset_manifest.csv
```

The six-workload counter summary is:

```text
experiments/paper-mascar/workloads/matrix/W7/w7_counter_summary.csv
```

## Counter Summary

| workload | M2 active | M3 active | M4 active | notes |
| --- | ---: | ---: | ---: | --- |
| `bfs` | 0 | 0 | 0 | no L1 saturated samples |
| `spmv` | 1 | 0 | 1 | activation observed |
| `mri_q` | 1 | 0 | 1 | activation observed |
| `pathfinder` | 1 | 0 | 1 | activation observed |
| `sgemm` | 0 | 0 | 0 | L1 samples but no saturated samples |
| `stencil` | 0 | 0 | 0 | L1 samples but no saturated samples |

Aggregate:

- M2 active workloads: 3
- M3 active workloads: 0
- M4 active workloads: 3
- any M2/M3/M4 active workloads: 3

## Interpretation

W7 succeeds in finding M2/M4 activation candidates for W8. It does not find M3 hit-only activation. Current evidence suggests that the stronger inputs can drive L1 saturation and M4 re-execution for `spmv`, `mri_q`, and `pathfinder`, but they do not create non-owner L1 hit-only accesses counted by the M3 path.

This is still smoke-scale input search. It is not a full Table III sweep and should not be reported as a paper performance trend.

## W8 Recommendation

Use the W7 activation-ready subset for the first W8 focused sweep:

- workloads: `spmv`, `mri_q`, `pathfinder`
- configs: `baseline_off`, `m2_owner_sched`, `m3_hitonly_nack`, `m4_reexec_load`
- retain timeout protection
- keep `completed_stats_found` separate from explicit correctness pass

Keep the inactive ready workloads as secondary coverage rows:

- `bfs`
- `sgemm`
- `stencil`

They are useful negative controls but should not be used for mechanism-benefit claims unless future inputs trigger active counters.
