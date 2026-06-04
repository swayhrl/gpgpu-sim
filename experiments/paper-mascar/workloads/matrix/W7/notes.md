# Mascar W7 Iteration Notes

## W7A Setup

Goal: strengthen the six W6 ready workload candidate inputs without changing M1-M4 Mascar mechanism code.

W7 iteration 1 uses the larger W5C inputs instead of the smaller W6/W5C iter2 inputs:

- `bfs`: 1024-node graph with 16 outgoing edges per node.
- `spmv`: 1024x1024 MatrixMarket matrix with 16 nonzeros per row.
- `mri_q`: 128 K values and 128 X values.
- `pathfinder`: direct parameters `256 256 4`.
- `sgemm`: generated 128x64 by 64x128 matrices.
- `stencil`: 16x16x16 binary grid with two iterations.

These inputs remain smoke-scale and are run only under the existing `m4_reexec_load` active config, which includes M2 owner scheduling and M3 hit-only/NACK controls.

## W7A Dry Run

Command:

```bash
DRY_RUN_ONLY=1 OUTDIR=experiments/paper-mascar/workloads/matrix/W7/results/iter1_dryrun bash experiments/paper-mascar/workloads/matrix/W7/run_w7_activation_search.sh
```

Result:

- rows: 6
- classification: `no_stats_exit0` for all dry-run rows
- wrapper parsing: pass

## W7A Actual Smoke

Command:

```bash
OUTDIR=experiments/paper-mascar/workloads/matrix/W7/results/iter1_actual W7_TIMEOUT_SEC=420 bash experiments/paper-mascar/workloads/matrix/W7/run_w7_activation_search.sh
```

Result:

- rows: 6
- completed rows: 6
- timeouts: 0
- crashes: 0
- explicit pass rows: 1 (`spmv`)
- stats-only completed rows: 5

## W7B Activation Result

W7 found three activation-ready workload candidates under `m4_reexec_load`:

- `spmv`: M2 active = 1, M3 active = 0, M4 active = 1
- `mri_q`: M2 active = 1, M3 active = 0, M4 active = 1
- `pathfinder`: M2 active = 1, M3 active = 0, M4 active = 1

Inactive under W7 iteration 1:

- `bfs`: L1 samples increased, but no saturated samples and no M2/M3/M4 active counters.
- `sgemm`: high L1 sample count, but no saturated samples and no M2/M3/M4 active counters.
- `stencil`: high L1 sample count, but no saturated samples and no M2/M3/M4 active counters.

M3 hit-only counters remain zero for all six workloads. W7 therefore provides M2/M4 activation candidates for W8, not M3 activation coverage.
