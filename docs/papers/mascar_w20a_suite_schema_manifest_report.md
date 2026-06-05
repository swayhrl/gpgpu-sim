# Mascar W20A Suite Schema and Manifest Report

## Goal
Normalize W19 Rodinia/Parboil artifacts into a stable full-suite manifest schema and status taxonomy.

## Inputs
W20 read W19 Rodinia, Parboil, command, build/data recovery, and smoke artifacts under `experiments/suites/common` and `experiments/suites/results`.

## Status taxonomy
Created `suite_status_taxonomy.md` and `suite_status_taxonomy.csv`. The taxonomy keeps ready, command-unverified, missing, failed, unsupported, placeholder, and unknown states distinct.

## Manifest schema
Created `suite_manifest_schema.md` and `normalize_w20_suite_manifests.py`. The normalized schema preserves source, binary, data, build, wrapper, command, status, correctness, blocker, and next-action evidence.

## Normalized outputs
- `full_suite_manifest.csv`: 41 rows
- `full_suite_ready_manifest.csv`: 13 rows
- `full_suite_blocker_manifest.csv`: 28 rows
- `full_suite_command_manifest.csv`: 41 rows
- `full_suite_manifest_quality_report.csv`: generated quality checks

## Ready baseline
Current ready baseline is 13; by suite: parboil:5;rodinia:8.

## Blockers
Blocker categories: binary_available_command_unverified:4;failed_smoke:4;source_available_missing_binary:20.

## Table III relation
Rows that overlap Mascar Table III are annotated, but W20 does not overwrite W18 proposed phase mapping.

## Limitation
W20 is not a build-recovery round. Failed-smoke and missing-binary rows remain blockers for W21.
