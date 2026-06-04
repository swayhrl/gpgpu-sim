# Mascar W6 Activation Sweep Report

## Executive Summary

W6 ran the ready workload candidate sweep requested after W5C. The sweep completed 6 ready Table III workloads across 4 configs, for 24 actual runs.

No activation-ready workload was found yet. Under the current W5C candidate inputs, W6 observed no M2/M3/M4 active mechanism counters: M2 active workloads = 0, M3 active workloads = 0, M4 active workloads = 0.

The baseline vs M2/M3/M4 cycle ratios in W6 are therefore smoke-level comparability checks only. They are not evidence of Mascar mechanism benefit and are not comparable to the paper speedup or energy claims.

## What W6 Ran

W6A selected the six W5C ready workload candidates:

- `bfs`
- `spmv`
- `mri_q`
- `pathfinder`
- `sgemm`
- `stencil`

W6B ran these workloads with:

- `baseline_off`
- `m2_owner_sched`
- `m3_hitonly_nack`
- `m4_reexec_load`

Total actual matrix rows: 6 workloads x 4 configs = 24 runs.

## Workload Coverage

The W6 coverage manifest keeps all 30 Table III rows in
`experiments/paper-mascar/workloads/results/W6/w6_coverage_manifest.csv`.

Coverage status:

- 30 Table III rows retained in coverage output.
- 6 ready candidates were run.
- 24 unavailable or unresolved rows were not run.
- 0 strict activation-ready rows were available.

Unavailable placeholder workloads were not executed as actual benchmarks.

## Config Coverage

The W6 config matrix is recorded in
`experiments/paper-mascar/workloads/matrix/W6/w6_config_matrix.csv`.

Enabled configs:

- `baseline_off`: baseline-safe config with Mascar disabled.
- `m2_owner_sched`: M2 owner scheduling active config.
- `m3_hitonly_nack`: M3 hit-only/NACK active config.
- `m4_reexec_load`: M4 load-only re-execution active config.

Probe-only configs are listed in the config matrix but disabled for W6.

## Status Classification

W6B latest summary:

- `completed_explicit_pass`: 4
- `completed_stats_found`: 20
- timeout: 0
- crash: 0

Only `completed_explicit_pass` rows have an explicit pass signal. `completed_stats_found` means the simulator completed and stats were collected, but correctness is not proven by an explicit workload pass line.

## Baseline Vs M2/M3/M4 Results

W6C writes per-run ratios to
`experiments/paper-mascar/workloads/results/W6/w6_trend_results.csv`.

The current smoke ratios are all 1.000000 geomean because active mechanism counters were not triggered and the active configs remained behaviorally equivalent for these inputs. This is a useful regression check, not a performance trend claim.

Geomean summary output:

- `m2_owner_sched`: all-ready cycle speedup geomean 1.000000, active workloads 0.
- `m3_hitonly_nack`: all-ready cycle speedup geomean 1.000000, active workloads 0.
- `m4_reexec_load`: all-ready cycle speedup geomean 1.000000, active workloads 0.

## Mechanism Activation

W6 activation summary is in
`experiments/paper-mascar/workloads/results/W6/w6_activation_summary.csv`.

Activation result:

- M2 active workload count: 0
- M3 active workload count: 0
- M4 active workload count: 0
- Any M2/M3/M4 active workload count: 0

Each ready workload had L1/M2 telemetry available, but none crossed into the active MP owner scheduling, hit-only/NACK, or re-execution paths under current inputs.

Conclusion: no activation-ready workload found yet / no M2-M4 activation under current inputs.

## Memory Vs Compute Trend

Ready memory-type workloads in W6:

- `bfs`
- `spmv`

Ready compute-type workloads in W6:

- `mri_q`
- `pathfinder`
- `sgemm`
- `stencil`

Because active mechanism counters are zero for both groups, W6 cannot report a memory-vs-compute Mascar trend. The memory and compute group geomeans are recorded only to show that the smoke matrix remained stable.

## Correctness-Pass Caveat

W6 does not treat `completed_no_explicit_pass` or `completed_stats_found` as correctness pass. In this run, explicit pass was observed only for the `spmv` rows. The other ready workload rows completed with stats, but did not emit an explicit pass signal recognized by the collector.

## Non-Triggering Workloads

The following ready candidates ran but did not trigger M2/M3/M4 active counters:

- `bfs`
- `spmv`
- `mri_q`
- `pathfinder`
- `sgemm`
- `stencil`

This matches the W5C finding that the six ready workloads are runnable candidates, not true activation-ready workloads.

## Failures And Timeouts

No actual W6B run timed out or crashed.

W6 implementation/debug notes:

- Fixed W6 runner repo-root resolution before the actual sweep.
- Added W6 command manifests because the common matrix runner expects the W2 command manifest schema, while the W6 run plan is an enriched planning schema.
- No wrapper runtime bug was found during the actual sweep.
- No localized M1-M4 Mascar runtime bug was found or modified.

## Comparison To Paper Expectations

W6 is not a paper-level reproduction run. It uses GPGPU-Sim 4.x with SM7/QV100-style configs, not the paper's GPGPU-Sim v3.2.2 GTX480/Fermi setup.

W6 did not run a full Rodinia/Parboil sweep and did not reproduce the paper's speedup or energy-saving claims. No energy model result was collected.

## Recommendations For W7/W8

Recommended next steps:

- Keep the six ready workloads as runtime smoke candidates.
- Continue activation input search before treating any subset as mechanism-ready.
- Increase L1 pressure or MP-mode opportunities in a bounded way, then rerun W6C activation analysis.
- Preserve the W6 distinction between runnable candidates and activation-ready workloads.
- Do not use W6 cycle ratios as evidence of Mascar benefit until M2/M3/M4 active counters are nonzero.
