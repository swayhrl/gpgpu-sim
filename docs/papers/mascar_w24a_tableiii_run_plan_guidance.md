# Mascar W24A Guidance: Table III Refreshed Sweep Run Plan

## Stage position

This is W24A of the Mascar Table III refreshed sweep.

Current state:
- M1-M4 Mascar mechanisms exist.
- W15 confirmed M3 implementation path is reachable via diagnostic workloads, but normal Table III inputs rarely trigger M3.
- W17 expanded Table III ready rows to about 15.
- W18 added kernel launch trace and inferred-order phase mapping for app-level phase-pending rows.
- W20/W21/W22/W23 created full-suite framework and baseline characterization infrastructure.
- W24 must now run a refreshed Table III sweep on current ready Table III rows.

W24A prepares the plan. W24B executes. W24C analyzes. W24D closes out.

## Goal

Create a precise Table III refreshed sweep plan covering all 30 paper rows, running actual simulations only for currently ready rows.

The run plan must preserve:

- paper row identity
- paper memory/compute classification
- W18 phase mapping status
- wrapper readiness
- measurement scope
- config matrix

## Important caveat

W18 provides kernel launch order/name and inferred phase mapping. Unless per-kernel stats delta is implemented, W24 measurements remain app-run level for wrappers that launch multiple kernels. Therefore W24 must include a `measurement_scope` field:

- exact_kernel_run
- app_run_inferred_phase
- app_run_unmapped
- unavailable

Do not claim paper-exact per-kernel performance if only app-level results are available.

## Hard constraints

1. Do not create a new branch.
2. Do not fetch upstream.
3. Do not modify Mascar M1-M4 mechanism behavior.
4. Do not overwrite canonical manifests destructively.
5. Keep all 30 Table III rows represented.
6. Do not actual-run unavailable rows.
7. Do not treat completed_no_explicit_pass as correctness pass.
8. Do not claim paper 34% speedup reproduction.
9. Do not claim GTX480/Fermi/GPGPU-Sim v3.2.2 equivalence.
10. Do not run Rodinia/Parboil full-suite; W24 is Table III focused.
11. Do not use git add . or git add -A.
12. Do not commit W24 outputs.
13. Use start_ts/end_ts in postcheck.
14. Raw logs must be archived under /workspace/tmp and not committed.

## Inputs to read

Read if present:

- experiments/paper-mascar/workloads/mascar_table_iii_workload_manifest.csv
- experiments/paper-mascar/workloads/mascar_table_iii_command_manifest.csv
- experiments/paper-mascar/workloads/matrix/W17/w17_ready_manifest.csv
- experiments/paper-mascar/workloads/matrix/W17/w17_unavailable_manifest.csv
- experiments/paper-mascar/workloads/matrix/W18/table_iii_phase_mapping_proposed_manifest.csv
- experiments/paper-mascar/workloads/matrix/W18/table_iii_phase_mapping.csv
- experiments/paper-mascar/workloads/results/W18/w18_latest_kernel_trace.csv
- experiments/suites/results/W23/w23_baseline_results.csv if present
- docs/papers/mascar_w18_kernel_trace_phase_mapping_report.md
- docs/papers/mascar_w23_baseline_characterization_report.md

If W18 mapping is absent:
- proceed with ready wrappers
- mark measurement_scope=app_run_unmapped
- document limitation

## Required directories

Create:

- experiments/paper-mascar/workloads/matrix/W24/
- experiments/paper-mascar/workloads/results/W24/
- experiments/paper-mascar/workloads/audit/W24/

## Config matrix

Create:

- experiments/paper-mascar/workloads/matrix/W24/w24_tableiii_config_matrix.csv

Rows:

1. baseline_off
   config_path=configs/hrl-repro/SM7_QV100_mascar_baseline_off
   config_role=baseline
   enabled=1

2. m2_owner_sched
   config_path=configs/hrl-repro/SM7_QV100_mascar_m2_owner_sched_on
   config_role=scheduling_only
   enabled=1

3. m3_hitonly_nack
   config_path=configs/hrl-repro/SM7_QV100_mascar_m3_hitonly_nack_on
   config_role=scheduling_hitonly
   enabled=1

4. m4_reexec_load
   config_path=configs/hrl-repro/SM7_QV100_mascar_m4_reexec_load_on
   config_role=scheduling_hitonly_reexec
   enabled=1

Optional disabled rows:
- m3diag_on enabled=0
- energy_m4 enabled=0

Do not enable old proxy scheduling by default.

## Planner script

Create:

- experiments/paper-mascar/workloads/matrix/W24/prepare_w24_tableiii_run_plan.py

Responsibilities:

1. Read canonical Table III workload manifest.
2. Read current command manifest.
3. Read W18 proposed phase mapping.
4. Generate an all-30-row W24 manifest.
5. Generate selected-ready manifest.
6. Generate unavailable manifest.
7. Generate run plan for selected ready rows x enabled configs.
8. Mark duplicate physical runs if multiple paper rows share the same wrapper/command.
9. Preserve measurement_scope and phase_mapping_confidence.

## Outputs

Create:

- experiments/paper-mascar/workloads/matrix/W24/w24_tableiii_manifest_all.csv
- experiments/paper-mascar/workloads/matrix/W24/w24_tableiii_manifest_ready.csv
- experiments/paper-mascar/workloads/matrix/W24/w24_tableiii_manifest_unavailable.csv
- experiments/paper-mascar/workloads/matrix/W24/w24_tableiii_phase_mapping_used.csv
- experiments/paper-mascar/workloads/matrix/W24/w24_tableiii_run_plan.csv
- experiments/paper-mascar/workloads/matrix/W24/w24_physical_run_groups.csv
- docs/papers/mascar_w24a_tableiii_run_plan.md
- experiments/paper-mascar/workloads/audit/W24/w24a_postcheck.md

## Required columns for all manifest

w24_tableiii_manifest_all.csv columns:

- paper_id
- paper_name
- paper_type
- paper_suite
- paper_app
- paper_kernel_or_phase
- paper_inst_per_l1_miss
- wrapper_path
- wrapper_status
- availability_status
- phase_mapping_status_w18
- phase_mapping_confidence_w18
- local_launch_index
- local_kernel_name
- measurement_scope
- selected_for_w24
- selection_reason
- correctness_status_prior
- notes

## Required run plan columns

w24_tableiii_run_plan.csv columns:

- run_id
- config_id
- config_path
- config_role
- paper_id
- paper_name
- paper_type
- wrapper_path
- measurement_scope
- phase_mapping_confidence_w18
- selected_for_run
- timeout_sec
- expected_activation
- duplicate_physical_group
- notes

## Selection rules

1. All 30 rows appear in all manifest.
2. Only rows with wrapper_status=ready are selected for actual run.
3. Unavailable rows remain in coverage manifest but are not actual-run.
4. If phase mapping is inferred_order, include row but mark measurement_scope=app_run_inferred_phase unless exact kernel-level stats exist.
5. If multiple rows share wrapper command, either:
   - run them separately, or
   - deduplicate physical run and copy results with duplicate_physical_group metadata.
   Running separately is acceptable if runtime is manageable.
6. Prefer clarity over complex dedup.

## W24A report

docs/papers/mascar_w24a_tableiii_run_plan.md must include:

1. Goal
2. Inputs used
3. W18 phase mapping status
4. Config matrix
5. Selected ready rows
6. Unavailable rows
7. Measurement scope caveat
8. Run row counts
9. Physical duplicate groups
10. W24B execution instructions

## Validation

Run:

- python3 -m py_compile experiments/paper-mascar/workloads/matrix/W24/prepare_w24_tableiii_run_plan.py
- python3 experiments/paper-mascar/workloads/matrix/W24/prepare_w24_tableiii_run_plan.py
- Check all manifest has exactly 30 rows.
- Check config matrix has 4 enabled core configs.
- Check run plan row count equals ready rows x 4 unless dedup is explicitly used.
- git diff --check

## Stop conditions

Stop W24A only if:
1. Table III manifest cannot be read.
2. Planner loses rows.
3. Config paths are missing and cannot be resolved.
4. Python unavailable.

Do not stop because some rows are unavailable.
