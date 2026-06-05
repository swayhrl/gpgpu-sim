# Mascar W18D Guidance: Paper Phase to Local Kernel Launch Mapping

## Stage position

This is W18D.

W18C collected kernel launch name/order. W18D maps paper Table III rows to local launch index/name with confidence levels.

## Goal

Create phase mapping for app-level ready rows:
- BP-1/BP-2
- histo-1/histo-2/histo-3
- kmeans-1/kmeans-2
- srad-1/srad-2

Do not pretend exactness where evidence is weak.

## Inputs

Read:

- experiments/paper-mascar/workloads/results/W18/w18_latest_kernel_trace.csv
- experiments/paper-mascar/workloads/mascar_table_iii_workload_manifest.csv
- experiments/paper-mascar/workloads/mascar_table_iii_command_manifest.csv
- experiments/paper-mascar/workloads/matrix/W17/w17_phase_pending_manifest.csv
- docs/papers/mascar_w17_workload_availability_expansion_report.md

## Mapping logic

Create script:

- experiments/paper-mascar/workloads/matrix/W18/infer_table_iii_phase_mapping.py

Rules:

1. Group rows by app.
2. Group trace by wrapper/app/run.
3. Identify launch sequence:
   - launch_index
   - kernel_name
   - grid/block
4. For paper rows with suffix numbers:
   - if number of launches matches number of paper rows, assign by launch order
   - if kernel names are descriptive, prefer name-based mapping
   - if one app row maps to many launches and paper rows fewer, mark inferred or unresolved
5. Use paper metadata:
   - paper_type memory/compute
   - paper_inst_per_l1_miss
   only as weak evidence unless per-kernel local stats are available.
6. If no kernel trace or only unknown names:
   - phase_mapping_status=trace_available_name_unknown or unresolved
7. Do not update canonical manifest destructively.

## Confidence levels

Use:

- exact
  Direct kernel name/order clearly matches paper phase and local stats support it.

- inferred_order
  Multiple launches and paper suffix rows are mapped by launch order.

- inferred_name
  Kernel name strongly suggests mapping.

- app_level_trace_available
  App run trace exists but cannot split paper phases.

- unresolved
  Not enough evidence.

- unavailable
  Workload not runnable.

## Outputs

Create:

- experiments/paper-mascar/workloads/matrix/W18/table_iii_phase_mapping.csv
- experiments/paper-mascar/workloads/matrix/W18/table_iii_phase_mapping_proposed_manifest.csv
- experiments/paper-mascar/workloads/matrix/W18/table_iii_phase_mapping_unresolved.csv
- docs/papers/mascar_w18d_phase_mapping_report.md

## table_iii_phase_mapping.csv columns

- paper_id
- paper_name
- paper_app
- paper_type
- paper_inst_per_l1_miss
- wrapper_path
- run_id
- local_launch_index
- local_kernel_uid
- local_kernel_name
- grid_x
- grid_y
- grid_z
- block_x
- block_y
- block_z
- mapping_status
- confidence
- evidence_type
- evidence_detail
- notes

## proposed manifest

table_iii_phase_mapping_proposed_manifest.csv should keep all 30 rows and add:

- local_launch_index
- local_kernel_name
- phase_mapping_status_w18
- phase_mapping_confidence_w18
- W18_notes

Do not overwrite original canonical manifests.

## Report sections

docs/papers/mascar_w18d_phase_mapping_report.md must include:

1. Goal
2. Trace inputs
3. Mapping method
4. Mapped rows
5. Unresolved rows
6. Confidence categories
7. BP/histo/kmeans/srad mapping discussion
8. How W18 improves W17
9. What remains for stricter per-kernel stat mapping
10. Caveats

## Validation

Run:

- python3 -m py_compile experiments/paper-mascar/workloads/matrix/W18/infer_table_iii_phase_mapping.py
- python3 experiments/paper-mascar/workloads/matrix/W18/infer_table_iii_phase_mapping.py
- Check proposed manifest has 30 rows.
- Check mapping CSV includes all phase-pending rows.
- Check unresolved CSV exists even if empty.

## Stop conditions

Stop only if:
1. kernel trace CSV is missing and cannot be regenerated
2. inference script loses rows
3. reports cannot be generated

Do not stop because mapping confidence is low. Low confidence must be reported honestly.
