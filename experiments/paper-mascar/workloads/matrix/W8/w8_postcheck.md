# Mascar W8A Postcheck

start_ts: 1780591778
end_ts: 1780591901
elapsed_sec: 123
start_iso: 2026-06-05T00:49:38+08:00
end_iso: 2026-06-05T00:51:41+08:00
branch: hrl/paper/mascar-repro-v0
head: 6921aee366c293f74df57a9bef129a980c98936d

## Git Status

```text
?? experiments/paper-mascar/workloads/matrix/W8/
```

## W8A Planner

Command:

```bash
python3 experiments/paper-mascar/workloads/matrix/W8/prepare_w8_run_plan.py
```

Outputs:

- w8_workload_manifest.csv rows: 30
- w8_command_manifest_subset.csv rows: 3
- w8_run_plan.csv rows: 12
- enabled configs: baseline_off, m2_owner_sched, m3_hitonly_nack, m4_reexec_load
- selected workloads: spmv, mri_q, pathfinder

## Notes

- W8 workload manifest preserves all Table III rows.
- Actual run manifest uses only the W7 activation-ready subset.
- docs/papers/mascar_w6a_activation_run_plan.md remains the W6 activation-plan reference document; W8 does not rewrite it.
