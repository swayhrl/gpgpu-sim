# Mascar W15D Guidance: M3 Diagnosis Report and Closeout

## Stage position

W15D closes W15.

It must decide whether M3 non-activation is:
- workload/input issue
- implementation bug
- config/wrapper issue
- unresolved

## Required report

Create:

- docs/papers/mascar_w15_m3_diagnosis_report.md

Required sections:

1. Executive summary
2. Why M3 matters
3. M3 activation conditions
4. W15A diagnostic counters added
5. W15B Table III diagnostic results
6. W15C microbenchmark results
7. Bug fixes made, if any
8. Final diagnosis
9. Remaining limitations
10. Recommendation for W16/W17

## Required final classification

Create:

- experiments/paper-mascar/workloads/matrix/W15/w15_m3_final_diagnosis.csv

Columns:
- test_scope
- workload_id
- config_id
- m3_activated
- main_blocker
- evidence_counter
- diagnosis
- next_action

main_blocker values:
- not_mp
- no_owner
- no_nonowner_load
- scheduler_blocked
- l1_probe_not_called
- l1_probe_no_hit
- config_issue
- wrapper_issue
- implementation_bug_fixed
- implementation_bug_unresolved
- microbenchmark_unavailable
- unknown

## Postcheck

Create:

- experiments/paper-mascar/workloads/audit/W15/w15_postcheck.md
- experiments/paper-mascar/workloads/audit/W15/w15_diff_name_status.txt
- experiments/paper-mascar/workloads/audit/W15/w15_symbol_grep.txt

Postcheck must include:
- start_iso
- end_iso
- elapsed_sec
- branch and HEAD
- build result
- configs created
- runs attempted
- W15B result
- W15C result
- whether M3 activated
- whether M3 implementation bug was found
- whether mechanism code was changed
- raw logs archive path
- review pack path
- warnings

## Review pack

Create:

- /workspace/tmp/mascar_w15_m3_diagnostic_review_pack_YYYYMMDD_HHMMSS.tar.gz

Include:
- source files changed, if any
- configs created
- microbenchmark source/wrapper
- matrix manifests/results summaries
- reports
- postcheck
- full diff patch

Archive raw logs to /workspace/tmp and do not commit huge logs.

## Validation

Run at end:
- git diff --check
- source setup_environment release && make -j2
- python3 -m py_compile collector scripts changed
- bash -n wrappers/scripts created
- grep paper_mascar_m3diag_
- run diagnostic collection if possible

## Final report to GPT

Report only:
1. elapsed_sec
2. review pack path
3. git status --short
4. whether M3 activated
5. if not activated, primary blocker
6. bug fixes made
7. files GPT should review
