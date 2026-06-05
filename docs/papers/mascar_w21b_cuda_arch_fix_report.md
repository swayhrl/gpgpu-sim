# Mascar W21B Legacy CUDA Arch / Compiler Fix Report

## Scope
W21B targeted source-backed Rodinia and Parboil blockers with CUDA 11.8 / gcc 11 compatibility strategies.

## Strategies attempted
- Override old arch flags with `sm_70` and `-allow-unsupported-compiler`.
- Use explicit `/usr/local/cuda` compiler/library paths.
- For Parboil, retry direct make on `cuda` and `cuda_base` implementations.

## Results
`build_results_w21b.csv` rows: 82; pass rows: 11.

## Key findings
Rodinia old Makefiles still hardcode unsupported `compute_20` or `sm_13` in several recipes, so variable overrides do not always replace the embedded flags. Parboil source-specific failures remain for BFS/FFT/imghisto/mri-fhd, including missing headers or CUDA 11 device-code errors.

## Outcome
W21B improved diagnosis and confirmed some build paths, but no blocker workload was promoted to ready solely from W21B.
