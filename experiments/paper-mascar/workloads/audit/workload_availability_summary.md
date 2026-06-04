# Mascar Table III Workload Availability Summary

Rows audited: 30

## Availability Counts

- available: 6
- missing_binary: 14
- missing_source: 1
- partial_phase_unknown: 9

## Phase Mapping Counts

- app_only: 9
- approximate: 6
- not_found: 15

## Ready Wrapper Candidates

- bfs -> rodinia_bfs
- spmv -> parboil_spmv
- mri-q -> parboil_mri_q
- pathfinder -> rodinia_pathfinder
- sgemm -> parboil_sgemm
- stencil -> parboil_stencil

## Placeholder Rows

- BP-2: partial_phase_unknown phase=app_only action=resolve_paper_phase_to_local_kernel_before_actual_table_iii_run
- histo-1: partial_phase_unknown phase=app_only action=resolve_paper_phase_to_local_kernel_before_actual_table_iii_run
- histo-3: partial_phase_unknown phase=app_only action=resolve_paper_phase_to_local_kernel_before_actual_table_iii_run
- kmeans-1: partial_phase_unknown phase=app_only action=resolve_paper_phase_to_local_kernel_before_actual_table_iii_run
- lbm: missing_binary phase=not_found action=build_or_import_binary
- leuko-1: missing_binary phase=not_found action=build_or_import_binary
- mrig-1: missing_binary phase=not_found action=build_or_import_binary
- mrig-2: missing_binary phase=not_found action=build_or_import_binary
- mummer: missing_binary phase=not_found action=build_or_import_binary
- particle: missing_binary phase=not_found action=build_or_import_binary
- sad-2: missing_binary phase=not_found action=build_or_import_binary
- srad-1: partial_phase_unknown phase=app_only action=resolve_paper_phase_to_local_kernel_before_actual_table_iii_run
- srad-2: partial_phase_unknown phase=app_only action=resolve_paper_phase_to_local_kernel_before_actual_table_iii_run
- BP-1: partial_phase_unknown phase=app_only action=resolve_paper_phase_to_local_kernel_before_actual_table_iii_run
- cutcp: missing_binary phase=not_found action=build_or_import_binary
- histo-2: partial_phase_unknown phase=app_only action=resolve_paper_phase_to_local_kernel_before_actual_table_iii_run
- histogram: missing_source phase=not_found action=import_or_point_to_benchmark_source
- kmeans-2: partial_phase_unknown phase=app_only action=resolve_paper_phase_to_local_kernel_before_actual_table_iii_run
- lavaMD: missing_binary phase=not_found action=build_or_import_binary
- leuko-2: missing_binary phase=not_found action=build_or_import_binary
- leuko-3: missing_binary phase=not_found action=build_or_import_binary
- mrig-3: missing_binary phase=not_found action=build_or_import_binary
- sad-1: missing_binary phase=not_found action=build_or_import_binary
- tpacf: missing_binary phase=not_found action=build_or_import_binary
