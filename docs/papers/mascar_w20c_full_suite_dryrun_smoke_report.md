# Mascar W20C Full-Suite Dry-Run and Smoke Report

## Goal
Validate the normalized full-suite framework end to end.

## Dry-run all
Dry-run all covered 41 / 41 normalized rows, including placeholders and blockers.

## Actual smoke ready
Smoke ready covered 13 / 13 current-ready rows with timeout protection and `RUN_PLACEHOLDERS=0`.

## Smoke classification
- `completed_explicit_pass`: 2
- stats-only or no-explicit-pass completion: 11
- timeout: 0
- crash: 0

Stats-only completion is simulator completion evidence, not correctness pass.

## Ready baseline
Ready baseline remains 13; by suite: parboil:5;rodinia:8.

## Blockers
Blocked rows remain in `full_suite_blocker_manifest.csv` and `w20_blocker_summary.csv`; categories: binary_available_command_unverified:4;failed_smoke:4;source_available_missing_binary:20.

## Raw logs
Raw run directories were archived to `/workspace/tmp/mascar_w20_raw_runs_20260605_160017.tar.gz` and removed from repo results.

## Limitation
W20C does not validate disabled M4/energy configs or blocked workloads.
