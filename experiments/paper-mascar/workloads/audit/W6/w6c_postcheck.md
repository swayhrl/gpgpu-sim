# W6C Postcheck

start_ts: 1780587417
end_ts: 1780588849
elapsed_sec: 1432
start_iso: 2026-06-04T23:36:57+08:00
end_iso: 2026-06-05T00:00:49+08:00
branch: hrl/paper/mascar-repro-v0
head: e193c9f9de3a1456934d34317675add1ba2a4dbe
review_pack: /workspace/tmp/mascar_w6_activation_sweep_review_pack_20260605_000049.tar.gz

## Git Status

```text
?? docs/papers/mascar_w6_activation_sweep_report.md
?? docs/papers/mascar_w6a_activation_run_plan.md
?? experiments/paper-mascar/workloads/audit/W6/
?? experiments/paper-mascar/workloads/matrix/W6/
?? experiments/paper-mascar/workloads/results/W6/
```

## Validation Status

- git diff --check: pass
- source setup_environment release && make -j2: pass, completed before W6B actual matrix
- python3 -m py_compile prepare_w6_run_plan.py analyze_w6_trends.py: pass
- bash -n run_w6_activation_sweep.sh: pass
- W6A planner: pass, 30 Table III rows, 6 ready rows, 0 strict activation-ready rows, 24 run-plan rows
- W6B dry-run ready matrix: pass, 24 rows
- W6B actual ready matrix: pass, 24 rows
- W6B activation-ready empty matrix: pass, explicit no-op because strict activation subset is empty
- W6C analyzer: pass, trend rows 24, activation rows 6, coverage rows 30
- CSV header and expected row counts: pass

## Build And Run Summary

- build_status: pass
- dry_run_status: pass
- actual_run_status: pass
- analyze_status: pass
- actual_runtime_workloads: yes, 6 ready candidate workloads
- actual_run_rows: 24
- timeout_rows: 0
- crash_rows: 0
- completed_explicit_pass_rows: 4
- completed_stats_found_rows: 20

## Workload And Config Counts

- Table III coverage rows: 30
- selected ready workloads: 6
- strict activation-ready workloads: 0
- enabled configs: 4
- ready sweep rows: 24
- activation-ready sweep rows: 0

## Active Counter Counts

- M2 active workloads: 0
- M3 active workloads: 0
- M4 active workloads: 0
- any M2/M3/M4 active workloads: 0

## Summary

W6 confirms the W5C conclusion: the six ready workloads are runnable candidates, not activation-ready workloads. No activation-ready workload was found yet and no M2-M4 activation was observed under current inputs. Baseline-vs-active cycle ratios are retained as smoke comparability checks only, not mechanism benefit or paper-comparable performance trends.

## Fixes During W6

- Fixed W6 runner repo-root resolution.
- Added W6 runner-compatible command manifests so the common matrix runner receives the W2 command schema.
- No wrapper runtime bug was found during actual runs.
- No M1-M4 Mascar mechanism code was modified.

## Warnings

- Do not treat completed_stats_found rows as correctness pass.
- Do not use W6 ratios as evidence of Mascar speedup or energy saving.
- W7/W8 need stronger bounded activation inputs before performance trend claims.

## Final Pack Timing Supplement

start_ts: 1780587417
end_ts: 1780588871
elapsed_sec: 1454
end_iso: 2026-06-05T00:01:11+08:00
review_pack: /workspace/tmp/mascar_w6_activation_sweep_review_pack_20260605_000049.tar.gz

## Raw Logs Archive

Large per-run raw logs were not committed to git. They were archived locally at:

```text
/workspace/tmp/mascar_w6_raw_runs_20260605_000920.tar.gz
```
