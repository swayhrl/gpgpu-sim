# Mascar W21A Parboil Build Driver / Python Compatibility Fix Report

## Scope
W21A targeted Parboil blocker workloads from W20. It did not modify Mascar M1-M4 behavior.

## Strategies attempted
- Strategy 1: run legacy Parboil driver under Python3 to confirm compatibility status.
- Strategy 2: bypass the Python2 driver using documented direct make variables.
- Strategy 3: create missing `common/Makefile.conf` from the local NVIDIA example and retry direct make.
- Strategy 4: retry direct make with the correct default make target and CUDA 11 paths.

## Results
`build_results_w21a.csv` rows: 78; pass rows: 19.

## Key findings
The top-level Parboil driver is Python2 syntax and fails under Python3. The local tree also lacked `common/Makefile.conf`. After adding it and using the correct default target, Parboil restored binaries for 9 benchmarks: cp;cutcp;lbm;mri-gridding;pns;rpes;sad;simple_mm;tpacf.

## Remaining blockers
Generated Parboil binaries are not promoted to ready because datasets and validated run commands are still missing/unverified.
