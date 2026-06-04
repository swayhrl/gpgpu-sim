# Mascar W7 Postcheck

start_ts: 1780590189
end_ts: 1780591089
elapsed_sec: 900
start_iso: 2026-06-05T00:23:09+08:00
end_iso: 2026-06-05T00:38:09+08:00
branch: hrl/paper/mascar-repro-v0
head: 83c12b4fcc96fe5f186182b67677752739c6f653
review_pack: /workspace/tmp/mascar_w7_activation_input_search_review_pack_20260605_003809.tar.gz

## Git Status

```text
?? docs/papers/mascar_w7_activation_input_search_report.md
?? experiments/paper-mascar/workloads/matrix/W7/
```

## W7A Commands

- dry-run: DRY_RUN_ONLY=1 OUTDIR=experiments/paper-mascar/workloads/matrix/W7/results/iter1_dryrun bash experiments/paper-mascar/workloads/matrix/W7/run_w7_activation_search.sh
- dry-run collect: python3 experiments/common/gpgpusim_matrix/collect_gpgpusim_stats.py experiments/paper-mascar/workloads/matrix/W7/results/iter1_dryrun
- actual smoke: OUTDIR=experiments/paper-mascar/workloads/matrix/W7/results/iter1_actual W7_TIMEOUT_SEC=420 bash experiments/paper-mascar/workloads/matrix/W7/run_w7_activation_search.sh
- actual collect: python3 experiments/common/gpgpusim_matrix/collect_gpgpusim_stats.py experiments/paper-mascar/workloads/matrix/W7/results/iter1_actual

## W7B Commands

- summarize: python3 experiments/paper-mascar/workloads/matrix/W7/summarize_w7_activation.py experiments/paper-mascar/workloads/matrix/W7/results/iter1_actual/results.csv

## Validation

- git diff --check: pass
- bash -n run_w7_activation_search.sh: pass
- bash -n W7 wrappers: pass
- python3 -m py_compile summarize_w7_activation.py: pass
- W7 dry-run rows: 6
- W7 actual rows: 6
- W7 counter summary rows: 6
- activation-ready subset rows: 3

## Runtime Status

- completed rows: 6
- timeout rows: 0
- crash rows: 0
- completed_explicit_pass rows: 1
- completed_stats_found rows: 5

## Counter Summary

- M2 active workloads: 3
- M3 active workloads: 0
- M4 active workloads: 3
- any active workloads: 3

## Activation-Ready Subset

- spmv: M2=1, M3=0, M4=1, active_config=m4_reexec_load
- mri_q: M2=1, M3=0, M4=1, active_config=m4_reexec_load
- pathfinder: M2=1, M3=0, M4=1, active_config=m4_reexec_load

## Non-Active Ready Workloads

- bfs: M2=0, M3=0, M4=0
- sgemm: M2=0, M3=0, M4=0
- stencil: M2=0, M3=0, M4=0

## Debug Notes

- No wrapper runtime bug required code changes.
- W5C larger SPMV input completed successfully in W7 with a 420-second timeout.
- M1-M4 Mascar mechanism code was not modified.
- Existing configs under configs/ were not modified.

## Outputs

- experiments/paper-mascar/workloads/matrix/W7/w7_counter_summary.csv
- experiments/paper-mascar/workloads/matrix/W7/activation_ready_subset_manifest.csv
- experiments/paper-mascar/workloads/matrix/W7/w7_latest_results.csv
- docs/papers/mascar_w7_activation_input_search_report.md

## Final Pack Timing Supplement

start_ts: 1780590189
end_ts: 1780591100
elapsed_sec: 911
end_iso: 2026-06-05T00:38:20+08:00
review_pack: /workspace/tmp/mascar_w7_activation_input_search_review_pack_20260605_003809.tar.gz

## Raw Logs Archive

Large per-run raw logs were not committed to git. They were archived locally at:

```text
/workspace/tmp/mascar_w7_raw_runs_20260605_004633.tar.gz
```
