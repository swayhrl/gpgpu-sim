# Mascar W6C Guidance: Trend Analysis and Coverage Closeout

## Stage position

This is W6C.

W6A created run plan.
W6B ran activation-aware ready workload sweep.
W6C must summarize results, compute preliminary trends, and decide whether W6 is sufficient to proceed to W7/W8 or needs more input work.

## Goals

1. Aggregate W6 results.
2. Compare baseline vs M2/M3/M4 for completed rows.
3. Compute speedup where valid.
4. Separate memory-intensive vs compute-intensive.
5. Identify which rows actually activate M2/M3/M4.
6. Produce a W6 report and next-step recommendation.

## Analysis script

Create:

- experiments/paper-mascar/workloads/matrix/W6/analyze_w6_trends.py

Inputs:
- W6 latest or combined results CSV
- W6 run plan
- Table III workload manifest

Outputs:

- experiments/paper-mascar/workloads/results/W6/w6_trend_results.csv
- experiments/paper-mascar/workloads/results/W6/w6_activation_summary.csv
- experiments/paper-mascar/workloads/results/W6/w6_geomean_summary.csv
- experiments/paper-mascar/workloads/results/W6/w6_trend_summary.md

Required calculations:

For each completed workload/config:

- cycles
- IPC
- result_status
- correctness_explicit_pass flag
- M2 active if m2_mp_cycles > 0 or owner_acquire > 0
- M3 active if m3_attempt > 0
- M4 active if m4_enqueue_success > 0 or m4_retry_attempt > 0

For speedup:
- baseline cycles must exist for same workload.
- config cycles must exist.
- speedup = baseline_cycles / config_cycles.
- do not compute speedup for unavailable/timeout/crash rows.
- mark correctness_unproven if no explicit pass.

Geomean:
- compute separately for memory-intensive and compute-intensive if at least 2 valid rows each.
- otherwise mark insufficient_data.
- also compute all-valid geomean if enough rows.

Trend flags:
- memory_workloads_with_m2_active
- memory_workloads_with_m4_active
- compute_workloads_with_degradation_gt_5pct
- m4_speedup_positive_count
- m4_speedup_negative_count
- rows_needing_input_expansion
- rows_needing_build_or_data

## Report

Create:

- docs/papers/mascar_w6_activation_sweep_report.md

Required sections:

1. Executive summary
2. What W6 ran
3. Workload coverage
4. Config coverage
5. Status classification
6. Baseline vs M2/M3/M4 results
7. Mechanism activation
8. Memory vs compute trend
9. Correctness-pass caveat
10. Non-triggering workloads
11. Failures/timeouts
12. Comparison to paper expectations
13. Recommendations for W7/W8

Important report constraints:
- Do not claim paper speedup reproduction.
- Do not claim energy reproduction.
- Do not claim correctness pass for no-explicit-pass rows.
- If only 6 ready workloads ran, state that coverage is still partial.
- If W5C activation-ready inputs improved activation, state evidence.
- If no M3/M4 activation happened, say W6 still lacks stress inputs.

## Coverage manifest update

Create:

- experiments/paper-mascar/workloads/results/W6/w6_coverage_manifest.csv

Columns:

- paper_id
- paper_name
- paper_type
- availability_status
- wrapper_status
- selected_for_w6
- baseline_status
- m2_status
- m3_status
- m4_status
- m2_active
- m3_active
- m4_active
- valid_for_trend
- correctness_status
- next_action

next_action values:
- use_in_W7
- needs_activation_input
- needs_build
- needs_data
- needs_phase_mapping
- needs_wrapper_fix
- unavailable
- drop_from_current_sweep

## W6 postcheck and review pack

Create:

- experiments/paper-mascar/workloads/audit/W6/w6c_postcheck.md
- experiments/paper-mascar/workloads/audit/W6/w6_diff_name_status.txt
- experiments/paper-mascar/workloads/audit/W6/w6_symbol_grep.txt

Postcheck must include:
- start_iso
- end_iso
- elapsed_sec
- branch and HEAD
- commands run
- build status
- run status
- analysis status
- row counts
- active mechanism counts
- files changed
- warnings
- review pack path

Review pack:

- /workspace/tmp/mascar_w6_activation_sweep_review_pack_YYYYMMDD_HHMMSS.tar.gz

Include:
- docs/papers/mascar_w6a_activation_run_plan.md
- docs/papers/mascar_w6_activation_sweep_report.md
- experiments/paper-mascar/workloads/matrix/W6/
- experiments/paper-mascar/workloads/results/W6/
- experiments/paper-mascar/workloads/audit/W6/
- full git diff patch

## Validation

Run:

- git diff --check
- python3 -m py_compile experiments/paper-mascar/workloads/matrix/W6/prepare_w6_run_plan.py
- python3 -m py_compile experiments/paper-mascar/workloads/matrix/W6/analyze_w6_trends.py
- bash -n experiments/paper-mascar/workloads/matrix/W6/run_w6_activation_sweep.sh
- Run analyzer on W6 latest/combined results.

## Stop conditions

Stop only if:
1. W6 result files are missing and cannot be regenerated.
2. analyzer corrupts CSV output.
3. build cannot be restored.
4. elapsed time exceeds 180 minutes.

Do not stop because speedup trend is weak.
Do not stop because no M3/M4 activation happened. Document it and recommend next input search.
