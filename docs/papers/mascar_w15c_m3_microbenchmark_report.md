# Mascar W15C M3 Microbenchmark Report

## Scope

W15C created `experiments/paper-mascar/workloads/micro/mascar_m3_diag/`, built a CUDA hot/cold-array diagnostic workload, and tried five input/config strategies.

## Build notes

The first binary was linked with static CUDA runtime and failed with `CUDA driver version is insufficient for CUDA runtime version`. The build script was fixed to use `nvcc -cudart shared`, allowing GPGPU-Sim's runtime to intercept the workload after `setup_environment release`.

## Strategies

1. Normal hot/cold arrays under `m3diag_on`.
2. Forced-MP config.
3. More blocks and outer iterations.
4. Smaller hot array and more reuse.
5. Larger cold working set and changed stride.

The first parameter set was too large for a bounded diagnostic run and timed out. The final recorded runs use smoke-sized variants of the same five strategies to keep W15C diagnostic time bounded.

## Results

All five final strategies completed and activated active M3 hit-only access. Strategy 1 under normal `m3diag_on` activated M3, and forced-MP strategies 2-5 activated M3 with larger active-hit-only counts.

## Diagnosis

The M3 implementation path is functional: scheduler non-owner load candidates reach LSU/L1D, hit-only probe calls are made, and active hit-only access counters increment. W15C does not show a localized M3 implementation bug.
