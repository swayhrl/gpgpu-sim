# Mascar W18E Guidance: Kernel Trace and Phase Mapping Closeout

## Stage position

This is W18E, final closeout for kernel launch trace and phase mapping.

## Goal

Produce a clean W18 review package and final report.

## Required closeout files

Create:

- docs/papers/mascar_w18_kernel_trace_phase_mapping_report.md
- experiments/paper-mascar/workloads/audit/W18/w18_postcheck.md
- experiments/paper-mascar/workloads/audit/W18/w18_diff_name_status.txt
- experiments/paper-mascar/workloads/audit/W18/w18_symbol_grep.txt
- experiments/paper-mascar/workloads/matrix/W18/w18_closeout_manifest.csv

## Final report sections

docs/papers/mascar_w18_kernel_trace_phase_mapping_report.md must include:

1. Executive summary
2. Why kernel trace is needed
3. Trace implementation
4. Trace configs
5. Workloads traced
6. Kernel launch results
7. Phase mapping results
8. Rows moved from app_level_pending to inferred/exact/unresolved
9. Remaining limitations
10. How W18 affects future Table III sweeps
11. Raw logs archive path
12. Recommended next work

## Postcheck

w18_postcheck.md must include:

- start_iso
- end_iso
- elapsed_sec
- branch and HEAD
- source changes
- config changes
- parser changes
- build status
- trace run status
- number of phase-pending rows traced
- number of mapped rows by confidence
- unresolved rows
- raw logs archive path
- review pack path
- warnings

## Raw logs

Archive raw W18 runs to:

- /workspace/tmp/mascar_w18_kernel_trace_raw_runs_YYYYMMDD_HHMMSS.tar.gz

Remove large runs directories before final git status if needed.

Keep summaries, kernel_trace.csv, mapping CSVs, reports.

## Review pack

Create:

- /workspace/tmp/mascar_w18_kernel_trace_review_pack_YYYYMMDD_HHMMSS.tar.gz

Include:
- source files changed
- trace configs
- common parser
- W18 matrix files
- W18 results summaries and kernel trace CSV
- W18 mapping CSVs
- W18 docs/reports
- W18 postcheck
- full git diff patch

## Validation

Run:

- git diff --check
- source setup_environment release && make -j2
- python3 -m py_compile experiments/common/gpgpusim_matrix/collect_kernel_trace.py
- python3 -m py_compile experiments/paper-mascar/workloads/matrix/W18/infer_table_iii_phase_mapping.py
- bash -n experiments/paper-mascar/workloads/matrix/W18/run_w18_kernel_trace_matrix.sh
- Check no huge raw logs are intended for git

## Final report to GPT

Report only:
1. elapsed_sec
2. review pack path
3. git status --short
4. whether simulator source was changed
5. number of traced workloads
6. number of mapped phase rows by confidence
7. unresolved rows
8. files GPT should review
