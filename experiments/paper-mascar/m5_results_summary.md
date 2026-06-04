# M5 Results Summary

Run directory: `/workspace/repos/gpgpu-sim_distribution/experiments/paper-mascar/m5_runs/20260604_185613`
Rows: 6
Completed: 6
Failed or timed out: 0

| config | workload | exit | cycles | ipc | l1 samples | m2 mp | m3 attempts | m4 enqueue | m4 retry |
|---|---|---:|---:|---:|---:|---:|---:|---:|---:|
| baseline_off | rodinia_hotspot | 0 | 6931 | 133.3510 | 0 | 0 | 0 | 0 | 0 |
| m1_l1sat_probe | rodinia_hotspot | 0 | 6931 | 133.3510 | 3095 | 0 | 0 | 0 | 0 |
| m2_owner_sched | rodinia_hotspot | 0 | 6931 | 133.3510 | 3095 | 495 | 0 | 0 | 0 |
| m3_hitonly_nack | rodinia_hotspot | 0 | 6931 | 133.3510 | 3095 | 495 | 0 | 0 | 0 |
| m4_reexec_load | rodinia_hotspot | 0 | 6918 | 133.6016 | 3498 | 631 | 0 | 53 | 440 |
| m4_reexec_probe_only | rodinia_hotspot | 0 | 6931 | 133.3510 | 3095 | 495 | 0 | 0 | 0 |

Interpretation notes:

- These are focused runtime sanity results, not paper-comparable speedup data.
- Zero M3 hit-only attempts can mean the short workload did not create MP non-owner load opportunities.
- M4 enqueue/retry nonzero values indicate the active load re-execution queue ran on this workload.
