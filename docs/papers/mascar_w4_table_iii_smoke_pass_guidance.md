# Mascar W4 Detailed Guidance: Table III Baseline + M4 Active Smoke Pass

## Stage position

This is W4 of the Mascar workload coverage effort.

W1/W2 created Table III workload inventory and wrappers.
W3 created the common matrix runner and collector.
W4 must run a Table III smoke pass using:

- baseline_off
- m4_reexec_load

The target is not performance conclusions yet. The target is coverage classification and debugging.

## Goal

Run or classify all 30 Table III rows under baseline and M4 active.

Expected run count:
- 30 workloads x 2 configs = 60 matrix rows.
- Placeholder wrappers may exit 77 quickly.
- Ready wrappers should be attempted unless runtime is clearly unsafe.

The final output must say exactly how many rows are:
- completed_explicit_pass
- completed_stats_found
- completed_no_explicit_pass
- completed_nonzero_with_stats
- wrapper_unavailable
- missing_binary
- missing_source
- missing_data
- phase_unknown
- timeout
- crash_assert
- simulator_error_no_stats

## Runtime policy

Use common runner:

- experiments/paper-mascar/workloads/matrix/run_mascar_table_iii_matrix.sh

Default timeout:
- 20 minutes per actual run.

If there are too many ready wrappers or runtime is long:
- start with MAX_RUNS limiting only if necessary.
- Prefer running all ready wrappers if there are only about 6 ready workloads, as W2 reported.
- Placeholders are cheap and should be included.

If a ready wrapper fails:
- Determine whether it is:
  - wrapper path/command bug
  - missing input/data
  - environment issue
  - simulator error
  - Mascar active regression
- Debug wrapper/config/script issues in this round.
- If M4 active crashes but baseline does not, inspect whether it is a Mascar bug. Fix if localized.
- Do not stop at first failure.

If a ready wrapper returns nonzero but stats exist:
- classify as completed_nonzero_with_stats.
- Do not call it pass.
- Keep logs for W5.

If no explicit PASS is printed:
- classify as completed_no_explicit_pass or completed_stats_found.
- Do not treat as correctness pass.

## Required W4 run

Run:

1. build/check:
   git diff --check
   source setup_environment release && make -j2

2. dry-run:
   DRY_RUN_ONLY=1 bash experiments/paper-mascar/workloads/matrix/run_mascar_table_iii_matrix.sh

3. collect dry-run:
   python3 experiments/common/gpgpusim_matrix/collect_gpgpusim_stats.py <dry-run-outdir>

4. actual smoke:
   bash experiments/paper-mascar/workloads/matrix/run_mascar_table_iii_matrix.sh

5. collect actual smoke:
   python3 experiments/common/gpgpusim_matrix/collect_gpgpusim_stats.py <actual-outdir>

If the actual smoke reveals wrapper bugs:
- fix wrappers.
- rerun affected workload/config pairs if feasible.
- If rerunning only failed subset is not implemented yet, rerun full smoke only if runtime is acceptable.

## Optional failed-only rerun support

If time permits, add to common runner:

- FILTER_PAPER_ID
- FILTER_CONFIG_ID
- ONLY_READY
- RERUN_FAILED_FROM

But do not overcomplicate. Simpler full rerun is acceptable if ready workload count is small.

## Required W4 result files

Create or copy from actual output into stable paths:

- experiments/paper-mascar/workloads/results/w4_smoke_latest_results.csv
- experiments/paper-mascar/workloads/results/w4_smoke_latest_summary.md
- experiments/paper-mascar/workloads/results/w4_smoke_latest_status_matrix.csv
- experiments/paper-mascar/workloads/results/w4_smoke_latest_run_manifest.csv

If actual outdir is timestamped, keep it and also copy/symlink stable latest files.

Create:

- docs/papers/mascar_w4_table_iii_smoke_report.md

Required sections:

1. Goal
2. Configs run
3. Workload coverage
4. Status classification
5. Ready wrapper results
6. Placeholder/unavailable rows
7. Baseline vs M4 active comparison at smoke level
8. M4 activation observations
9. Failures/timeouts/crashes
10. Fixes made during W4
11. Recommendations for W5 activation screening

## Smoke-level interpretation

Do not claim performance trend yet.

Allowed claims:

- "This workload completed and produced stats."
- "This workload produced no explicit pass marker."
- "M4 active had nonzero re-exec stats."
- "M4 active did not trigger on this smoke input."
- "This row is unavailable due missing binary/data/source."
- "This row remains phase-mapping unresolved."

Not allowed:

- "Mascar improves X by Y%" as final conclusion.
- "This reproduces the paper speedup."
- "This workload passes correctness" unless explicit pass or benchmark-specific validation confirms it.

## Debug expectations

If W4 actual smoke finds:

1. wrapper command bug:
   fix wrapper.

2. missing data:
   mark missing_data; do not invent.

3. missing binary but source exists:
   if build command is obvious and bounded, run build helper for that workload.
   else mark available_needs_build or missing_binary.

4. baseline works but M4 active crashes:
   debug active M4 path.
   Fix localized issue.
   Rerun build and that workload/config if feasible.

5. both baseline and M4 active fail similarly:
   classify as workload/env issue unless log indicates common simulator bug.

## Required postcheck

Create:

- experiments/paper-mascar/workloads/audit/w4_postcheck.md
- experiments/paper-mascar/workloads/audit/w3_w4_diff_name_status.txt
- experiments/paper-mascar/workloads/audit/w3_w4_symbol_grep.txt

w4_postcheck.md must include:

- start_iso
- end_iso
- elapsed_sec
- branch and HEAD
- git status before and after
- commands run
- build status
- dry-run status
- actual smoke status
- number of matrix rows
- number of completed rows
- number of unavailable placeholders
- number of no-explicit-pass rows
- number of nonzero-with-stats rows
- number of timeouts/crashes
- fixes made
- review pack path
- warnings

## Review pack

Create:

- /workspace/tmp/mascar_w3_w4_review_pack_YYYYMMDD_HHMMSS.tar.gz

Include:

- experiments/common/gpgpusim_matrix/
- experiments/paper-mascar/workloads/matrix/
- experiments/paper-mascar/workloads/results/
- experiments/paper-mascar/workloads/wrappers/ if changed
- experiments/paper-mascar/workloads/audit/w3_postcheck.md
- experiments/paper-mascar/workloads/audit/w4_postcheck.md
- docs/papers/mascar_w3_matrix_runner_framework.md
- docs/papers/mascar_w4_table_iii_smoke_report.md
- full git diff patch

If logs are too large:
- include run manifests, results, summaries, and compressed tails.
- keep raw logs in repo working tree if small; otherwise include only in review pack and document path.

## Validation commands

At end run:

- git diff --check
- bash -n experiments/common/gpgpusim_matrix/run_gpgpusim_matrix.sh
- python3 -m py_compile experiments/common/gpgpusim_matrix/collect_gpgpusim_stats.py
- bash -n experiments/paper-mascar/workloads/matrix/run_mascar_table_iii_matrix.sh
- python3 -m py_compile experiments/paper-mascar/workloads/matrix/collect_mascar_table_iii_matrix.py

Do not run full benchmark suites beyond W4 smoke matrix.

## Stop conditions

Stop only if:

1. build cannot be restored.
2. common runner corrupts manifests or logs.
3. multiple ready workloads show the same likely Mascar mechanism crash and localized fix is not possible.
4. elapsed time exceeds 130 minutes.
5. repository state becomes unsafe.

Do not stop because placeholders return 77.
Do not stop because a workload has no explicit PASS.
Do not stop because one wrapper needs debugging.
