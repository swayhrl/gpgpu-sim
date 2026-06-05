# Mascar W17A Missing Workload Audit

## Goal

Audit all 30 Mascar Table III rows for local source, binary, data, build, wrapper, and phase-mapping gaps. W17A does not run the full benchmark suite and does not modify Mascar mechanisms.

## Inputs

- `experiments/paper-mascar/workloads/mascar_table_iii_workload_manifest.csv`
- `experiments/paper-mascar/workloads/mascar_table_iii_workload_manifest_audited.csv`
- `experiments/paper-mascar/workloads/mascar_table_iii_command_manifest.csv`
- `experiments/paper-mascar/workloads/mascar_workload_availability_summary.csv`
- `/workspace/repos/gpgpu-workloads/manifests/workload_manifest.csv`

## Result

W17A preserved all 30 Table III rows.

- Existing exact/approx ready wrappers: 7
- App-level phase-unknown rows that can be normalized in W17C: 8
- Missing-binary rows with local source/build candidates: 14
- Missing-source rows: 1 (`histogram`)

## Key Findings

- Phase-suffixed rows for BP, histo, kmeans, and srad have local app-level commands and should be promoted to ready only with `app_level_pending_kernel_trace` status.
- Raw source exists locally for lbm, leukocyte, mri-gridding, mummergpu, particlefilter, sad, cutcp, lavaMD, and tpacf.
- `histogram` remains source-blocked; Rodinia `hybridsort/histogram1024_kernel.cu` is only a low-confidence kernel fragment candidate, not a complete app-level Table III source.

## Outputs

- `w17a_candidate_inventory.csv`
- `w17a_gap_matrix.csv`
- `w17a_next_action_plan.csv`
- `w17a_candidate_paths.txt`
