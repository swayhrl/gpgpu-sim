# Mascar W8C Postcheck

- start_ts: 1780591778
- end_ts: 1780595336
- elapsed_sec: 3558
- branch: hrl/paper/mascar-repro-v0
- HEAD: 6921aee366c293f74df57a9bef129a980c98936d

## Analyzer Outputs

- trend rows: 12
- activation summary rows: 3
- M2 active workloads: 3
- M3 active workloads: 0
- M4 active workloads: 3
- any active workloads: 3
- missing planned pairs: 0

Generated files:

- `experiments/paper-mascar/workloads/matrix/W8/w8_trend_results.csv`
- `experiments/paper-mascar/workloads/matrix/W8/w8_activation_summary.csv`
- `experiments/paper-mascar/workloads/matrix/W8/w8_geomean_summary.csv`
- `experiments/paper-mascar/workloads/matrix/W8/w8_trend_summary.md`
- `docs/papers/mascar_w8_activation_trend_report.md`

## Summary

M2 and M4 counters activate for `spmv`, `mri_q`, and `pathfinder`; M3 hit-only counters remain zero. The focused sweep shows identical `gpu_tot_sim_cycle` values across baseline/M2/M3/M4 for the three workloads, so no performance trend or paper-comparable speedup is claimed.

## Validation

- `git diff --check`: pass
- W8 run plan rows: 12
- W8 latest result rows: 12
- W8 trend rows: 12
- W8 activation summary rows: 3

## Raw Logs Archive

Large W8 per-run raw logs were not committed to git. They were archived locally at:

```text
/workspace/tmp/mascar_w8_raw_runs_20260605_015514.tar.gz
```

Only latest CSV/summary/status manifests and configfix summary files are intended for git.

## Raw Logs Archive

Large W8 per-run raw logs were not committed to git. They were archived locally at:

```text
/workspace/tmp/mascar_w8_raw_runs_20260605_015524.tar.gz
```

Only latest CSV/summary/status manifests and configfix summary files are intended for git.
