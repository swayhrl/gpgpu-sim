# W24 Final Postcheck

start_ts=1780652070
end_ts=1780653683
elapsed_sec=1613
start_iso=2026-06-05T17:34:30+08:00
end_iso=2026-06-05T18:01:23+08:00
branch=hrl/paper/mascar-repro-v0
head=aa7a228
postcheck_method=start_ts=$(date +%s) / end_ts=$(date +%s)

selected_ready_rows=15
coverage_rows=30
actual_run_rows=60
completed_rows=60
timeout_rows=0
crash_rows=0
classification_counts={'completed_no_explicit_pass': 44, 'completed_explicit_pass': 16}
M2_active_rows=4
M3_active_rows=0
M4_active_rows=4
geomean_summary=all:m2_owner_sched=0.999671(15); all:m3_hitonly_nack=0.999671(15); all:m4_reexec_load=0.999729(15); memory:m2_owner_sched=0.999510(8); memory:m3_hitonly_nack=0.999510(8); memory:m4_reexec_load=0.999564(8); compute:m2_owner_sched=0.999855(7); compute:m3_hitonly_nack=0.999855(7); compute:m4_reexec_load=0.999917(7)
raw_log_archive=/workspace/tmp/mascar_w24_raw_runs_20260605_180025.tar.gz
validation=git_diff_check_pass;py_compile_pass;bash_n_pass;planner_pass;dryrun_pass;actual_sweep_pass;analyzer_pass;coverage_30_pass;raw_archived;no_pycache
warnings=no_paper_speedup_claim;no_energy_claim;inferred_order_not_exact;app_run_not_strict_per_kernel

## Raw Logs Archive

Large W24 per-run raw logs were not committed to git. They were archived locally at:

```text
/workspace/tmp/mascar_w24_raw_runs_20260605_181244.tar.gz
```

## Raw Logs Archive

Large W24 per-run raw logs were not committed to git. They were archived locally at:

```text
/workspace/tmp/mascar_w24_raw_runs_20260605_181249.tar.gz
```
