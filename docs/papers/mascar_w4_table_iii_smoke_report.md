# Mascar W4 Table III Smoke Report

## Goal

W4 runs a smoke-level Table III coverage pass using the W3 matrix framework. This is not a performance reproduction and does not claim paper-equivalent inputs or speedups.

## Configs Run

- `baseline_off`: `configs/hrl-repro/SM7_QV100_mascar_baseline_off`
- `m4_reexec_load`: `configs/hrl-repro/SM7_QV100_mascar_m4_reexec_load_on`

## Workload Coverage

The matrix included all 30 Table III workload rows from the W1/W2 command manifest. With two configs, the actual smoke matrix contained 60 rows.

`rodinia_hotspot` was not included because it is not a Table III workload.

## Corrected Status Classification

The collector was corrected in W4 review because the initial PASS regex treated ordinary lowercase English `pass` in simulator configuration comments as an explicit benchmark pass. The corrected classifier only treats explicit uppercase PASS/PASSED markers or explicit pass phrases as correctness pass markers.

Final W4 classification after correction:

- `completed_explicit_pass`: 2
- `completed_no_explicit_pass`: 10
- `completed_stats_found`: 0
- `completed_nonzero_with_stats`: 0
- `missing_binary`: 28
- `missing_source`: 2
- `missing_data`: 0
- `phase_unknown`: 18
- `timeout`: 0
- `crash_assert`: 0
- `simulator_error_no_stats`: 0

The latest stable result files are:

- `experiments/paper-mascar/workloads/results/w4_smoke_latest_results.csv`
- `experiments/paper-mascar/workloads/results/w4_smoke_latest_summary.md`
- `experiments/paper-mascar/workloads/results/w4_smoke_latest_status_matrix.csv`
- `experiments/paper-mascar/workloads/results/w4_smoke_latest_run_manifest.csv`

## Ready Wrapper Results

The six ready wrappers completed under both baseline and M4 active:

- `bfs`
- `mri_q`
- `pathfinder`
- `sgemm`
- `spmv`
- `stencil`

Of these 12 ready rows, `spmv` under both configs is classified as `completed_explicit_pass`. The other 10 ready rows are classified as `completed_no_explicit_pass`. Only the `spmv` rows should be treated as having an explicit pass marker; the remaining completed rows produced simulator stats but should not be used as correctness-pass evidence.

## Placeholder Unavailable Rows

Placeholder rows are expected and are not failures. Counts are doubled because each unavailable Table III workload row is represented once for baseline and once for M4 active:

- `phase_unknown`: 18
- `missing_binary`: 28
- `missing_source`: 2

## Baseline vs M4 Active Comparison At Smoke Level

By config:

### `baseline_off`

- `completed_explicit_pass`: 1
- `completed_no_explicit_pass`: 5
- `missing_binary`: 14
- `missing_source`: 1
- `phase_unknown`: 9

### `m4_reexec_load`

- `completed_explicit_pass`: 1
- `completed_no_explicit_pass`: 5
- `missing_binary`: 14
- `missing_source`: 1
- `phase_unknown`: 9


No baseline-only or M4-only crash/timeout was observed.

## M4 Activation Observations

The M4 smoke inputs did not trigger nonzero M4 re-execution queue stats:

- total `paper_mascar_m4_enqueue_success` over M4 rows: 0
- total `paper_mascar_m4_retry_attempt` over M4 rows: 0
- total `paper_mascar_m2_mp_cycles` over M4 rows: 0

This means W4 confirms that M4 active can run these smoke workloads, but it does not prove that these inputs exercise the active re-execution path.

## Failures, Timeouts, Crashes

No timeout, crash/assertion, missing-data failure, simulator-error-no-stats row, or nonzero-with-stats row was observed.

## Fixes Made During W4 Review

- Fixed the collector explicit-pass regex to avoid matching ordinary lowercase `pass`.
- Re-ran the collector on dry-run and actual W4 output.
- Regenerated latest result CSVs, summaries, status matrix, and this report.

## Recommendations For W5 Activation Screening

W5 should focus on:

- Adding bounded wrappers or build steps for source-only Table III rows.
- Resolving BP, histo, kmeans, and srad phase mappings.
- Selecting or constructing smoke inputs that actually trigger M2/M3/M4 active counters.
- Keeping `completed_no_explicit_pass` separate from explicit correctness pass.
