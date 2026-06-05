# Mascar W24A Table III Run Plan

## Goal
Run a refreshed current-simulator Table III sweep over currently ready rows only.

## Inputs used
Canonical Table III workload manifest, W17 ready/unavailable manifests, W18 proposed phase mapping, and current command manifest.

## W18 phase mapping status
W18 inferred launch-order mapping is used where present. `inferred_order` remains inferred and is not exact.

## Config matrix
Enabled configs: baseline_off, m2_owner_sched, m3_hitonly_nack, m4_reexec_load.

## Selected ready rows
Selected ready rows: 15. Paper IDs: bp_2, bfs, histo_1, histo_3, kmeans_1, spmv, srad_1, srad_2, bp_1, histo_2, kmeans_2, mri_q, pathfinder, sgemm, stencil.

## Unavailable rows
Unavailable rows: 15. They remain in coverage manifest and are not actual-run.

## Measurement scope caveat
Rows with inferred launch-order mapping are measured at app-run level and marked `app_run_inferred_phase`; this is not strict per-kernel paper phase timing.

## Run row counts
Actual planned rows: 60 = 15 ready rows x 4 enabled configs.

## Physical duplicate groups
Physical groups: 15. Duplicate commands are run separately for clarity.

## W24B execution instructions
Run `bash experiments/paper-mascar/workloads/matrix/W24/run_w24_tableiii_refreshed_sweep.sh` after dry-run validation.
