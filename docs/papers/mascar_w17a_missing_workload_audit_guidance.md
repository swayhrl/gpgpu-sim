# Mascar W17A Guidance: Missing Table III Workload Audit

## Stage position

This is W17A of the Mascar Table III workload coverage effort.

Current state:
- Mascar M1-M4 mechanism implementation exists.
- W1-W2 created Table III 30-row workload inventory and wrappers.
- W3-W4 created matrix runner and smoke classification.
- W5-W15 explored activation, diagnostics, and framework results.
- W16 built the current-simulator energy trend pipeline.
- W17 now expands Table III workload availability.

W17A is audit-only. It must discover source, binary, data, build commands, and wrapper gaps for all non-ready Table III rows.

## Goal

Increase Table III workload coverage by identifying what is needed to make missing / placeholder / phase_unknown rows runnable.

W17A must not build aggressively yet. It prepares exact candidates for W17B/W17C.

## Hard constraints

1. Do not create a new branch.
2. Do not fetch upstream.
3. Do not modify M1-M4 Mascar mechanism code.
4. Do not run full benchmark suites in W17A.
5. Do not download external data/source unless explicitly available locally.
6. Do not fabricate commands for missing workloads.
7. Keep all 30 Table III rows represented.
8. Do not collapse phase rows such as BP-1/BP-2 or histo-1/2/3 into one exact row.
9. Use app-level or approximate phase status when exact kernel/phase is unknown.
10. Do not use git add . or git add -A.
11. Do not commit W17 outputs.
12. Raw logs and scans must stay concise or be archived.

## Table III rows that need attention

Start from the current command/availability manifests. Do not hard-code this list if manifests disagree, but the known not-yet-ready rows likely include:

- lbm
- leuko-1
- mrig-1
- mrig-2
- mummer
- particle
- sad-2
- cutcp
- histogram
- lavaMD
- leuko-2
- leuko-3
- mrig-3
- sad-1
- tpacf

Also re-audit all phase_unknown rows even if wrappers exist.

## Required input files

Read:

- experiments/paper-mascar/workloads/mascar_table_iii_workload_manifest.csv
- experiments/paper-mascar/workloads/mascar_table_iii_workload_manifest_audited.csv
- experiments/paper-mascar/workloads/mascar_table_iii_command_manifest.csv
- experiments/paper-mascar/workloads/mascar_workload_availability_summary.csv
- docs/papers/mascar_w1_workload_inventory_report.md
- docs/papers/mascar_w2_workload_command_normalization_report.md
- docs/papers/mascar_w13_table_iii_coverage_report.md if present

## Search roots

Search local roots only:

- /workspace/repos/gpgpu-workloads
- /workspace/repos/gpgpu-sim_simulations
- /workspace/repos/gpgpu-sim_distribution
- /workspace/repos
- /workspace

Skip:

- .git
- review_packs
- m5_runs
- results raw runs directories
- logs directories unless explicitly needed
- __pycache__
- large tar/zip outputs

## Aliases to search

Use workload-specific aliases.

Examples:

- backprop: backprop, bp
- bfs: bfs
- histo/histogram: histo, histogram
- kmeans: kmeans
- lavaMD: lava, lavamd, lavaMD
- lbm: lbm
- leukocyte: leukocyte, leuko
- mri-q: mri-q, mriq
- mrig: mri-gridding, mrig, gridding
- mummer: mummer
- particle: particlefilter, particle
- sad: sad
- spmv: spmv
- srad: srad
- stencil: stencil
- tpacf: tpacf
- cutcp: cutcp
- pathfinder: pathfinder
- sgemm: sgemm

## Required outputs

Create directories:

- experiments/paper-mascar/workloads/matrix/W17/
- experiments/paper-mascar/workloads/audit/W17/
- experiments/paper-mascar/workloads/results/W17/

Create:

- experiments/paper-mascar/workloads/matrix/W17/w17a_missing_workload_audit.py
- experiments/paper-mascar/workloads/matrix/W17/w17a_candidate_inventory.csv
- experiments/paper-mascar/workloads/matrix/W17/w17a_gap_matrix.csv
- experiments/paper-mascar/workloads/matrix/W17/w17a_next_action_plan.csv
- experiments/paper-mascar/workloads/audit/W17/w17a_candidate_paths.txt
- docs/papers/mascar_w17a_missing_workload_audit.md
- experiments/paper-mascar/workloads/audit/W17/w17a_postcheck.md

## Candidate inventory columns

w17a_candidate_inventory.csv columns:

- paper_id
- paper_name
- paper_type
- current_availability_status
- current_wrapper_status
- current_phase_mapping_status
- alias_used
- candidate_kind
- candidate_path
- has_source
- has_binary
- has_makefile
- has_cmake
- has_build_script
- has_data
- likely_suite
- confidence
- notes

candidate_kind values:

- source_dir
- binary_file
- data_dir
- build_script
- makefile
- wrapper
- config
- unknown

confidence values:

- high
- medium
- low

## Gap matrix columns

w17a_gap_matrix.csv columns:

- paper_id
- paper_name
- paper_type
- current_status
- primary_gap
- secondary_gap
- candidate_source
- candidate_binary
- candidate_data
- candidate_build_command
- phase_mapping_status
- recommended_W17B_action
- recommended_W17C_action
- notes

primary_gap values:

- missing_binary
- missing_source
- missing_data
- phase_unknown
- wrapper_missing
- command_unknown
- unsupported_env
- already_ready
- unknown

## Next action plan columns

w17a_next_action_plan.csv columns:

- priority
- paper_id
- action_type
- action_detail
- expected_risk
- expected_time
- can_attempt_in_W17B
- can_attempt_in_W17C
- notes

Priority rules:

1. source and Makefile found, missing binary
2. binary found, wrapper missing or wrong
3. data found, command unknown
4. phase_unknown but app-level ready
5. missing_source or unsupported_env

## Phase mapping policy

W17 may improve app-level availability, but exact paper phase mapping belongs mainly to W18.

Allowed phase statuses in W17:

- exact
- inferred
- app_level_pending_kernel_trace
- phase_unknown
- unavailable

If only an app-level command is available for BP-1/BP-2 or histo-1/2/3, mark:

- app_level_pending_kernel_trace

Do not call it exact.

## Report requirements

docs/papers/mascar_w17a_missing_workload_audit.md must include:

1. Goal
2. Current ready/unavailable counts
3. Search roots
4. Candidate source/binary/data findings
5. Per-workload gap summary
6. High-priority build candidates
7. High-priority wrapper candidates
8. Phase mapping caveats
9. What W17B should attempt
10. What W18 must still solve

## Validation

Run:

- python3 -m py_compile experiments/paper-mascar/workloads/matrix/W17/w17a_missing_workload_audit.py
- python3 experiments/paper-mascar/workloads/matrix/W17/w17a_missing_workload_audit.py
- Check all 30 Table III rows appear in gap matrix.
- Check candidate inventory and next action CSV have headers.
- Check report exists.

## Stop conditions

Stop W17A only if:

1. Table III manifest cannot be read.
2. Audit script loses rows.
3. Python cannot run.
4. Repository state becomes unsafe.

Do not stop because many workloads are missing. Missing workloads are expected and must be documented.
