# Mascar W17D Guidance: Availability Expansion Closeout

## Stage position

This is W17D, closeout for W17.

W17A audited missing workloads.
W17B attempted builds/recovered binaries.
W17C updated wrappers/command manifest and smoke-tested them.
W17D summarizes coverage progress and prepares handoff to W18 kernel launch trace.

## Goal

Produce a clean closeout package:
- updated ready counts
- unresolved workloads and reasons
- newly ready wrappers
- phase mapping caveats
- W18 action list

## Required final manifests

Create:

- experiments/paper-mascar/workloads/matrix/W17/w17_ready_manifest.csv
- experiments/paper-mascar/workloads/matrix/W17/w17_unavailable_manifest.csv
- experiments/paper-mascar/workloads/matrix/W17/w17_phase_pending_manifest.csv
- experiments/paper-mascar/workloads/matrix/W17/w17_next_action_for_W18.csv

w17_ready_manifest.csv columns:

- paper_id
- paper_name
- paper_type
- wrapper_path
- availability_status
- wrapper_status
- phase_mapping_status
- binary_path
- data_path
- smoke_status
- correctness_status
- notes

w17_unavailable_manifest.csv columns:

- paper_id
- paper_name
- paper_type
- blocker
- candidate_source
- candidate_binary
- candidate_data
- attempted_actions
- next_action
- notes

w17_phase_pending_manifest.csv columns:

- paper_id
- app
- current_wrapper
- phase_mapping_status
- expected_kernel_trace_need
- notes

w17_next_action_for_W18.csv columns:

- priority
- paper_id
- app
- phase_question
- wrapper_path
- expected_kernel_count
- trace_needed
- notes

## Final report

Create:

- docs/papers/mascar_w17_workload_availability_expansion_report.md

Required sections:

1. Executive summary
2. Starting coverage before W17
3. W17A audit results
4. W17B build/binary recovery results
5. W17C wrapper normalization results
6. Final ready/unavailable counts
7. Newly ready workloads
8. Still unavailable workloads and reasons
9. Phase mapping caveats
10. W18 kernel launch trace handoff
11. Raw log/archive note
12. Limitations

## Postcheck

Create:

- experiments/paper-mascar/workloads/audit/W17/w17_postcheck.md
- experiments/paper-mascar/workloads/audit/W17/w17_diff_name_status.txt
- experiments/paper-mascar/workloads/audit/W17/w17_symbol_grep.txt

w17_postcheck.md must include:

- start_iso
- end_iso
- elapsed_sec
- branch and HEAD
- git status before/after
- commands run
- build attempts count
- wrapper updates count
- ready count before/after
- placeholder count before/after
- phase_pending count
- actual smoke status
- raw logs archive path
- review pack path
- warnings

## Review pack

Create:

- /workspace/tmp/mascar_w17_availability_review_pack_YYYYMMDD_HHMMSS.tar.gz

Include:
- W17 reports
- W17 matrix files
- W17 audit files
- changed wrappers
- updated command manifest
- build/smoke result summaries
- full git diff patch

Do not include huge raw logs in git.
Archive raw logs to /workspace/tmp.

## Validation

Run:

- git diff --check
- python3 -m py_compile W17 Python scripts
- bash -n W17 shell scripts
- bash -n changed wrappers
- Check manifests have 30 rows where expected
- Check no raw large logs are staged
- Check report exists

## Stop conditions

Stop only if:
1. final manifests cannot be generated.
2. W17 loses Table III row identity.
3. existing ready wrappers are broken and cannot be restored.
4. repository state is unsafe.

Do not stop because coverage is still less than 30/30. Document blockers and next actions.
