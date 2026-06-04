# Mascar W6A Activation Run Plan

## Goal

W6A prepares an activation-aware run plan for the currently runnable Table III workloads. It uses W5C as candidate-input evidence, not as a strict activation-ready subset, because W5C found no workload with nonzero M2/M3/M4 active counters.

## Inputs From W5 W5C

Inputs read:

- `experiments/paper-mascar/workloads/mascar_table_iii_workload_manifest.csv`
- `experiments/paper-mascar/workloads/mascar_table_iii_command_manifest.csv`
- `experiments/paper-mascar/workloads/matrix/W5A/activation_counters.csv`
- `experiments/paper-mascar/workloads/matrix/W5B/activation_matrix.csv`
- `experiments/paper-mascar/workloads/matrix/W5B/subset_manifest.csv`
- `experiments/paper-mascar/workloads/matrix/W5C/activation_ready_subset_manifest.csv`
- `experiments/paper-mascar/workloads/matrix/W5C/w5c_iter2_command_manifest.csv`
- `experiments/paper-mascar/workloads/results/w4_smoke_latest_results.csv`

W5C status:

- Six ready workloads ran.
- No strict M2/M3/M4 activation was observed.
- W6 therefore treats the six rows as ready workload candidates.

## Config Matrix

W6 enables four configs:

- `baseline_off`
- `m2_owner_sched`
- `m3_hitonly_nack`
- `m4_reexec_load`

Probe-only configs are listed but disabled.

## Workload Selection Policy

Selection rules:

- Keep all 30 Table III rows in `w6_workload_manifest_all_tableiii.csv`.
- Run only `wrapper_status=ready` rows by default.
- Do not run placeholders or unavailable rows in W6B.
- Prefer W5C candidate input commands for the six ready rows, while marking strict activation as not found.
- Keep `completed_no_explicit_pass` separate from explicit correctness pass in W6C.

## Activation Ready Subset

`w6_workload_manifest_activation_ready.csv` is intentionally empty. W5C did not find any workload with M2/M3/M4 active counters.

## Ready Wrappers

Ready candidate workloads selected for W6:

- `bfs`
- `spmv`
- `mri_q`
- `pathfinder`
- `sgemm`
- `stencil`

## Unavailable Table III Rows

The remaining Table III rows are retained in the all-table manifest as phase-unknown or missing-source/binary rows. They are not scheduled for W6B actual benchmark runs.

## Run Plan Summary

W6 run plan:

- workloads: 6
- configs: 4
- planned actual rows: 24
- strict activation-ready rows: 0

## Risks And Assumptions

The W5C candidate inputs may still fail to trigger M2/M3/M4 activation. If that happens, W6C must report no activation-ready workload found yet and must not claim performance trends as mechanism benefit.

## How W6B Should Execute

W6B should run:

```bash
W6_MODE=ready bash experiments/paper-mascar/workloads/matrix/W6/run_w6_activation_sweep.sh
```

Each actual run must use a timeout. Placeholders remain skipped.
