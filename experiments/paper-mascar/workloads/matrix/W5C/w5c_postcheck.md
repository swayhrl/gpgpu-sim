# W5C Postcheck

start_ts: 1780585521
end_ts: 1780586303
elapsed_sec: 782
start_iso: 2026-06-04T23:05:21+08:00
end_iso: 2026-06-04T23:18:23+08:00
branch: hrl/paper/mascar-repro-v0
head: fa4954a4d36ebf8a38a86a51567efbad4b1df3db

## Git Status

```text
 M experiments/common/gpgpusim_matrix/run_gpgpusim_matrix.sh
?? docs/papers/mascar_w5c_activation_ready_inputs_report.md
?? experiments/paper-mascar/workloads/matrix/W5C/
```

## Summary

```text
rows 6
classifications {'completed_stats_found': 5, 'completed_explicit_pass': 1}
M2_active 0
M3_active 0
M4_active 0
```

## Commands Run

- W5C dry-run matrix for 6 ready workloads.
- W5C actual iter1 partial smoke; stopped after enlarged inputs were too slow for W5C screening.
- W5C actual iter2 smoke for all 6 ready workloads under m4_reexec_load.
- python3 experiments/common/gpgpusim_matrix/collect_gpgpusim_stats.py experiments/paper-mascar/workloads/matrix/W5C/results/actual_iter2
- git diff --check
- bash -n W5C wrappers and command helpers
- python3 -m py_compile experiments/common/gpgpusim_matrix/collect_gpgpusim_stats.py

## Warnings

- No generated W5C input triggered M2 MP owner, M3 hit-only, or M4 re-exec active counters.
- activation_ready_subset_manifest.csv preserves all 6 ready workloads with active flags set to 0; it is negative screening evidence, not an activation-ready full-sweep subset.
- Common matrix runner was fixed to pass COMMAND_MANIFEST into wrappers; no Mascar mechanism code was changed.

## Review Pack

review_pack_path: /workspace/tmp/mascar_w5c_review_pack_20260604_231823.tar.gz

## Raw Logs Archive

Large per-run raw logs were not committed to git. They were archived locally at:

```text
/workspace/tmp/mascar_w5c_raw_results_20260604_232952.tar.gz
```
