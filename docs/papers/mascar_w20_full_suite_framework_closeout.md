# Mascar W20 Full-Suite Framework Closeout

## Executive summary
W20 converted W19 Rodinia/Parboil audit artifacts into a reusable full-suite framework. Current ready baseline remains 13 workloads.

## W20 inputs from W19
W19 manifests, build/data recovery CSVs, and smoke results were read. W20 did not assume W19 added new ready workloads.

## Normalized manifests
`full_suite_manifest.csv` has 41 rows: 23 Rodinia and 18 Parboil.

## Runner and collector
`run_full_suite_matrix.sh` wraps the common GPGPU-Sim matrix runner. `collect_full_suite_results.py` wraps the common collector and adds suite summaries.

## Dry-run and smoke validation
Dry-run all covered 41 rows. Smoke ready covered 13 rows.

## Current ready baseline
Current ready baseline: 13; by suite: parboil:5;rodinia:8.

## Blocker categories
binary_available_command_unverified:4;failed_smoke:4;source_available_missing_binary:20

## How this supports Mascar
The framework provides stable Rodinia/Parboil baselines for future Mascar sweeps without changing M1-M4 behavior.

## How this supports future papers
The normalized schema, taxonomy, runner, collector, and raw-log policy are reusable for new paper reproduction workflows.

## Next recommended round W21
W21 should focus on build and command repair for blocker rows, especially legacy CUDA flags, Parboil invocation paths, and missing data/runtime dependencies.

## Limitations
W20 does not repair blocked workloads, does not run M4/energy full-suite configs, and does not claim strict Table III phase alignment.
