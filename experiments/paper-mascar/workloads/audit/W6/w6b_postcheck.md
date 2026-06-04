# W6B Postcheck

start_iso: 2026-06-04T23:36:57+08:00
end_iso: 2026-06-04T23:55:42+08:00
elapsed_sec: 1125
branch: hrl/paper/mascar-repro-v0
head: e193c9f9de3a1456934d34317675add1ba2a4dbe

## Git Status

```text
?? docs/papers/mascar_w6a_activation_run_plan.md
?? experiments/paper-mascar/workloads/audit/W6/
?? experiments/paper-mascar/workloads/matrix/W6/
?? experiments/paper-mascar/workloads/results/W6/
```

## Build Status

pass: source setup_environment release && make -j2

## Dry Run Status

pass: 24 dry-run rows collected under experiments/paper-mascar/workloads/results/W6/w6_ready_dryrun

## Actual Run Status

pass: W6 ready candidate sweep completed.

```text
rows 24
classifications {'completed_stats_found': 20, 'completed_explicit_pass': 4}
configs ['baseline_off', 'm2_owner_sched', 'm3_hitonly_nack', 'm4_reexec_load']
workloads ['bfs', 'mri_q', 'pathfinder', 'sgemm', 'spmv', 'stencil']
timeouts 0
crashes 0
```

## Output Dirs

- dryrun_outdir: experiments/paper-mascar/workloads/results/W6/w6_ready_dryrun
- actual_outdir: experiments/paper-mascar/workloads/results/W6/w6_ready_sweep_actual

## Failures And Fixes

- Fixed W6 runner repo_root path.
- Added W6 runnable command manifest generation because W6A enriched manifest schema is not directly compatible with the common runner input schema.
- No wrapper runtime bug or Mascar runtime bug was found.

## Warnings

- W6 uses W5C candidate inputs; strict activation-ready subset is empty.
- No M2/M3/M4 active counters observed in W6B collector output.

## Final Numeric Timing Supplement

start_ts: 1780587417
end_ts: 1780588849
elapsed_sec: 1432

## Final Pack Timing Supplement

start_ts: 1780587417
end_ts: 1780588871
elapsed_sec: 1454
