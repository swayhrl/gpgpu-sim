# W5B Postcheck

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

activation_matrix.csv covers all 30 Table III workloads.
subset_manifest.csv intentionally has zero workload rows because mechanism_active rows are zero.

## Warnings

- Empty subset_manifest.csv means W6/W7 strict mechanism-active full-sweep subset is not ready from current smoke inputs.
- The six ready telemetry-only rows should be used for activation input search, not paper speedup claims.
- No wrapper, runner, config, or mechanism bug was fixed in W5.

## Review Pack

review_pack_path: /workspace/tmp/mascar_w5_review_pack_20260604_214507.tar.gz
