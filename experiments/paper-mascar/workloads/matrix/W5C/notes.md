# Mascar W5C Iteration Notes

## Goal

Generate activation-ready inputs for the six Table III ready workloads:

- `bfs`
- `spmv`
- `mri_q`
- `pathfinder`
- `sgemm`
- `stencil`

The W5 baseline showed L1/M2 telemetry only. M2 MP owner, M3 hit-only, and M4 re-exec counters stayed zero because the smoke inputs did not drive recent L1 saturation.

## Iteration 1 Input Changes

- `bfs`: generated a 1024-node graph with 16 outgoing edges per node.
- `spmv`: generated a 1024x1024 MatrixMarket matrix with 16 nonzeros per row and a matching vector.
- `mri_q`: generated a binary input with 128 K values and 128 X values.
- `pathfinder`: increased direct parameters to `256 256 4`.
- `sgemm`: generated 128x64 by 64x128 matrices.
- `stencil`: generated a 16x16x16 binary grid and ran two iterations.

These are smoke-scale increases, not full benchmark inputs.

## Iteration 1 Runtime Notes

`actual_iter1` was stopped after the first enlarged BFS/SPMV attempts became too slow for W5C small-smoke screening. Partial logs showed L1/M2 telemetry but no M2 MP owner, M3 hit-only, or M4 re-exec activation before termination.

## Iteration 2 Input Changes

To keep the full six-workload pass bounded, W5C generated smaller but still enlarged inputs:

- `bfs`: 256-node graph with 8 outgoing edges per node.
- `spmv`: 256x256 MatrixMarket matrix with 8 nonzeros per row and a matching vector.
- `mri_q`: 64 K values and 64 X values.
- `pathfinder`: direct parameters `128 128 4`.
- `sgemm`: 128x32 by 32x64 generated matrices.
- `stencil`: 12x12x12 binary grid with two iterations.

## Iteration 2 Runtime Result

`actual_iter2` completed all six workloads under `m4_reexec_load`.

Result:

- M2 MP owner active counters: not triggered.
- M3 hit-only active counters: not triggered.
- M4 re-exec active counters: not triggered.

Conclusion: W5C did not identify activation-ready Table III inputs under the existing M4 active config. The generated inputs are useful negative evidence for W6/W7 planning, but should not be treated as activation-ready.
