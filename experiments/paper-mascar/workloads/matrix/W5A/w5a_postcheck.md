# W5A Postcheck

start_ts: 1780580565
end_ts: 1780580707
elapsed_sec: 142
start_iso: 2026-06-04T21:42:45+08:00
end_iso: 2026-06-04T21:45:07+08:00
branch: hrl/paper/mascar-repro-v0
head: fda252e56b03b4f9b073753ce85f2f4727a20d49

## Git Status

```text
?? docs/papers/mascar_w5b_activation_matrix_report.md
?? experiments/paper-mascar/workloads/matrix/W5A/
?? experiments/paper-mascar/workloads/matrix/W5B/
```

## Summary

```text
counter_rows 30
counter_tiers {'phase_unknown': 9, 'telemetry_only': 6, 'missing_binary': 14, 'missing_source': 1}
mechanism_active_rows 0
activation_matrix_rows 30
subset_rows 0
```

W5A source evidence: W4 latest baseline/M4 smoke results and W4 per-run logs.

## Warnings

- No Table III smoke workload triggered M2 MP owner, M3 hit-only, or M4 re-exec counters.
- Six ready workloads only showed L1/M2 telemetry counters.
- Placeholder/unavailable rows are preserved and not counted active.
