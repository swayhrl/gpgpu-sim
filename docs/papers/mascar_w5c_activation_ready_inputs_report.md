# Mascar W5C Activation-Ready Input Report

## Goal

W5C attempted to create activation-ready inputs for the six currently runnable Table III workloads:

- `bfs`
- `spmv`
- `mri_q`
- `pathfinder`
- `sgemm`
- `stencil`

The goal was to trigger M2 MP owner scheduling, M3 non-owner hit-only/NACK, or M4 load re-execution counters without modifying Mascar M1-M4 mechanisms.

## Scope

W5C changed only workload input data and command parameters through W5C-specific manifests and wrapper shims. It did not modify Mascar mechanism code and did not run a full benchmark suite.

## Inputs Generated

Iteration 1 generated larger inputs, but the first attempts were too slow for W5C smoke screening and still showed no active counters in partial logs.

Iteration 2 used bounded smoke inputs:

- `bfs`: 256-node graph, degree 8.
- `spmv`: 256x256 sparse matrix, degree 8.
- `mri_q`: generated 64 K by 64 X binary input.
- `pathfinder`: `128 128 4`.
- `sgemm`: generated 128x32 by 32x64 matrices.
- `stencil`: generated 12x12x12 grid with two iterations.

## Counter Results

All six iteration-2 runs completed under `m4_reexec_load`. None triggered strict active counters:

- M2 active: 0 workloads.
- M3 active: 0 workloads.
- M4 active: 0 workloads.

The generated W5C subset manifest is:

- `experiments/paper-mascar/workloads/matrix/W5C/activation_ready_subset_manifest.csv`

It contains all six ready workloads with active flags set to `0`. This is deliberate negative evidence, not a claim that activation-ready inputs were found.

## Debug Fixes

W5C fixed one runner/wrapper plumbing issue: the common matrix runner now passes `COMMAND_MANIFEST=${WORKLOAD_MANIFEST}` into wrappers. This lets W5C wrapper shims use W5C-specific command manifests instead of falling back to the W2 default manifest.

No Mascar M1-M4 mechanism code was changed.

## W6 W7 Recommendation

Do not use these W5C inputs as activation-ready full-sweep inputs. Use them as bounded negative screening inputs and continue activation search with either:

- stronger memory-pressure workload inputs,
- source-level workload wrappers for currently missing Table III rows,
- or a controlled activation-probe micro workload outside Table III, kept separate from paper workload claims.

W6/W7 should not claim paper-speedup reproduction from these inputs.
