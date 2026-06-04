# W6A Postcheck

start_iso: 2026-06-04T23:36:57+08:00
branch: hrl/paper/mascar-repro-v0
head: e193c9f9de3a1456934d34317675add1ba2a4dbe

## Git Status

```text
?? docs/papers/mascar_w6a_activation_run_plan.md
?? experiments/paper-mascar/workloads/matrix/W6/
?? experiments/paper-mascar/workloads/results/W6/
```

## W5C Input Status

W5C activation_ready_subset_manifest.csv is present but contains zero strict activation-ready rows. W6 treats the six W5C rows as ready workload candidates only.

## Row Counts

- all_tableiii_rows: 30
- ready_rows: 6
- strict_activation_ready_rows: 0
- enabled_config_rows: 4
- run_plan_rows: 24

## Selected Workloads

- bfs
- spmv
- mri_q
- pathfinder
- sgemm
- stencil

## Warnings

- No activation-ready workload found yet.
- Current W6 run plan is ready-candidate coverage, not a paper-comparable full Table III sweep.

## Final Numeric Timing Supplement

start_ts: 1780587417
end_ts: 1780588849
elapsed_sec: 1432

## Final Pack Timing Supplement

start_ts: 1780587417
end_ts: 1780588871
elapsed_sec: 1454
