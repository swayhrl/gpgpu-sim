# Mascar W20A Guidance: Full-Suite Schema and Manifest Normalization

## Stage position

This is W20A of the Rodinia/Parboil full-suite framework closeout.

Current state:
- W19 audited local Rodinia/Parboil full-suite availability.
- W19 produced Rodinia and Parboil manifests, suite command manifest, wrapper smoke results, and blocker summaries.
- W19 did not necessarily increase the final smoke-ready workload count; current framework must treat W19's 13 smoke-ready rows as the current runnable baseline.
- W20 must turn W19's one-round artifacts into stable, reusable infrastructure.

W20A normalizes manifests and status schemas. W20B stabilizes runner/collector. W20C runs dry-run/smoke validation. W20D writes closeout docs and review pack.

## Goal

Create a stable full-suite manifest schema and status taxonomy for Rodinia/Parboil workloads.

The result must support:
- Table III paper rows
- Rodinia full-suite rows
- Parboil full-suite rows
- Ready workloads
- Command-unverified workloads
- Missing binary/source/data rows
- Build-failed rows
- Unsupported legacy CUDA rows
- Placeholder rows
- Future papers and user experiments

## Hard constraints

1. Do not create a new branch.
2. Do not fetch upstream.
3. Do not modify Mascar M1-M4 mechanism behavior.
4. Do not run full benchmark suites in W20A.
5. Do not discard W19 evidence.
6. Do not collapse Rodinia/Parboil app identity with Mascar Table III paper_id identity.
7. Do not overwrite W19 manifests destructively without writing normalized copies.
8. Keep unavailable and placeholder rows; do not drop them.
9. Do not use git add . or git add -A.
10. Do not commit W20 outputs.
11. Use start_ts/end_ts in postcheck.
12. If a field cannot be determined, mark unknown with notes.

## Required W19 inputs

Read if present:

- experiments/suites/common/rodinia_full_manifest.csv
- experiments/suites/common/parboil_full_manifest.csv
- experiments/suites/common/suite_command_manifest.csv
- experiments/suites/common/build_results.csv
- experiments/suites/common/binary_recovery_results.csv
- experiments/suites/common/data_availability_results.csv
- experiments/suites/common/w19_smoke_results.csv
- docs/papers/mascar_w19a_full_suite_inventory_report.md
- docs/papers/mascar_w19b_build_recovery_report.md
- docs/papers/mascar_w19c_wrapper_command_normalization_report.md
- docs/papers/mascar_w19d_smoke_validation_report.md

Also read common framework files from previous rounds:

- experiments/common/gpgpusim_matrix/run_gpgpusim_matrix.sh
- experiments/common/gpgpusim_matrix/collect_gpgpusim_stats.py
- experiments/paper-mascar/workloads/mascar_table_iii_workload_manifest.csv
- experiments/paper-mascar/workloads/mascar_table_iii_command_manifest.csv

## Required directories

Create or update:

- experiments/suites/common/
- experiments/suites/audit/W20/
- experiments/suites/results/W20/
- docs/workload_suites/

## Status taxonomy

Create:

- experiments/suites/common/suite_status_taxonomy.md
- experiments/suites/common/suite_status_taxonomy.csv

Required status values:

- ready
- command_verified
- command_unverified
- binary_available_command_unverified
- source_available_missing_binary
- missing_binary
- missing_source
- missing_data
- failed_build
- failed_smoke
- unsupported_cuda
- unsupported_dependency
- unsupported_runtime
- phase_pending
- placeholder
- unknown

For each status, define:
- meaning
- whether runner should dry-run it
- whether runner should actual-run it by default
- whether it counts as ready
- typical next action

## Full-suite manifest schema

Create:

- experiments/suites/common/suite_manifest_schema.md

Define canonical full-suite manifest columns:

- suite_id
- suite_name
- app_id
- app_name
- benchmark_name
- variant
- source_path
- binary_path
- data_path
- build_command
- run_command
- wrapper_path
- wrapper_status
- availability_status
- build_status
- data_status
- command_status
- smoke_status
- correctness_status
- input_scale
- default_timeout_sec
- current_ready
- table_iii_related
- table_iii_paper_ids
- phase_mapping_status
- blocker
- next_action
- notes

## Normalized manifests

Create script:

- experiments/suites/common/normalize_w20_suite_manifests.py

Responsibilities:
1. Read W19 Rodinia and Parboil manifests.
2. Read W19 suite command manifest.
3. Merge into one normalized full-suite manifest.
4. Preserve original source fields when possible.
5. Fill unknown fields explicitly.
6. Assign current_ready based on wrapper_status / smoke_status / command status.
7. Produce blocker classification.
8. Preserve Table III relation if known.
9. Generate quality report.

Outputs:

- experiments/suites/common/full_suite_manifest.csv
- experiments/suites/common/full_suite_command_manifest.csv
- experiments/suites/common/full_suite_blocker_manifest.csv
- experiments/suites/common/full_suite_ready_manifest.csv
- experiments/suites/common/full_suite_manifest_quality_report.csv

## full_suite_manifest_quality_report.csv columns

- check_name
- status
- count
- detail
- recommended_action

Required checks:
- row_count_total
- rodinia_rows
- parboil_rows
- ready_rows
- missing_binary_rows
- missing_source_rows
- failed_build_rows
- command_unverified_rows
- rows_missing_wrapper_path
- rows_missing_notes
- duplicate_app_ids
- duplicate_wrapper_paths
- table_iii_related_rows

## W20A report

Create:

- docs/papers/mascar_w20a_suite_schema_manifest_report.md

Required sections:
1. Goal
2. W19 input summary
3. Status taxonomy
4. Full-suite manifest schema
5. Normalized manifest outputs
6. Ready count
7. Blocker count
8. Relation to Mascar Table III
9. Risks and limitations
10. What W20B should use

## W20A postcheck

Create:

- experiments/suites/audit/W20/w20a_postcheck.md

Include:
- start_iso
- end_iso
- elapsed_sec
- branch and HEAD
- W19 files found/missing
- commands run
- row counts
- ready count
- blocker count
- files created
- warnings

## Validation

Run:
- python3 -m py_compile experiments/suites/common/normalize_w20_suite_manifests.py
- python3 experiments/suites/common/normalize_w20_suite_manifests.py
- Check full_suite_manifest.csv exists.
- Check full_suite_command_manifest.csv exists.
- Check full_suite_ready_manifest.csv exists.
- Check no expected W19 rows are lost.
- git diff --check

## Stop conditions

Stop W20A only if:
1. W19 manifests cannot be read and no fallback input exists.
2. normalization script loses rows.
3. Python cannot run.
4. repository state becomes unsafe.

Do not stop because ready count is low. W20 is framework closeout, not blocker repair.
