# Mascar W5B Activation Matrix Report

## Goal

W5 screens Table III workloads for Mascar mechanism activation using the existing baseline and M4-active smoke results. This stage does not run full benchmark suites and does not modify M1-M4 mechanism code.

## Inputs

W5 used the W4 smoke artifacts:

- `experiments/paper-mascar/workloads/results/w4_smoke_latest_results.csv`
- `experiments/paper-mascar/workloads/results/w4_smoke_latest_run_manifest.csv`
- W4 per-run logs under `experiments/paper-mascar/workloads/results/w4_smoke_actual/runs/`

The Table III row source remains:

- `experiments/paper-mascar/workloads/mascar_table_iii_command_manifest.csv`

## Activation Definition

W5 separates telemetry from mechanism activation:

- `telemetry_only`: L1/M2 telemetry counters are nonzero, but MP owner scheduling, M3 hit-only, and M4 re-exec counters are zero.
- `mechanism_active`: at least one of these is nonzero:
  - M2 MP/owner scheduling counters such as `paper_mascar_m2_mp_cycles`, `paper_mascar_m2_owner_acquire`, or `paper_mascar_m2_nonowner_mem_block`.
  - M3 non-owner hit-only/NACK counters such as `paper_mascar_m3_hitonly_access_attempt`.
  - M4 re-execution counters such as `paper_mascar_m4_enqueue_success` or `paper_mascar_m4_retry_attempt`.

Placeholder rows are preserved and are not counted active.

## W5A Counter Summary

W5A produced:

- `experiments/paper-mascar/workloads/matrix/W5A/activation_counters.csv`
- `experiments/paper-mascar/workloads/matrix/W5A/activation_summary.md`

Activation tier counts:

- `telemetry_only`: 6
- `phase_unknown`: 9
- `missing_binary`: 14
- `missing_source`: 1
- `mechanism_active`: 0

The six ready rows had L1/M2 telemetry, but all M2 MP owner, M3 hit-only, and M4 re-exec counters were zero.

## W5B Matrix Summary

W5B produced:

- `experiments/paper-mascar/workloads/matrix/W5B/activation_matrix.csv`
- `experiments/paper-mascar/workloads/matrix/W5B/subset_manifest.csv`

`activation_matrix.csv` covers all 30 Table III rows.

`subset_manifest.csv` contains no workload rows because no Table III smoke workload triggered the mechanism-active criteria. It intentionally contains only the CSV header.

## Workload Tiers

Ready but telemetry-only rows:

- `bfs`
- `spmv`
- `mri_q`
- `pathfinder`
- `sgemm`
- `stencil`

Phase-mapping unresolved rows:

- `BP-1`
- `BP-2`
- `histo-1`
- `histo-2`
- `histo-3`
- `kmeans-1`
- `kmeans-2`
- `srad-1`
- `srad-2`

Unavailable rows:

- 14 rows are `missing_binary`.
- 1 row is `missing_source`.

## W6 W7 Recommendation

The current Table III subset for full sweep is empty under strict mechanism-active criteria. Before W6/W7 full sweep, the next useful step is activation input search:

- Keep the six ready telemetry-only workloads as candidates for larger or more stressful inputs.
- Resolve phase mappings for BP, histo, kmeans, and srad rows.
- Add bounded wrappers or builds for source-only rows.
- Continue to report `completed_no_explicit_pass` separately from explicit correctness pass.

## Limitations

W5 is based on smoke-scale W4 runs, not paper-scale Table III experiments. Zero M2/M3/M4 activation on these inputs does not prove the mechanism cannot activate on larger or paper-equivalent inputs.
