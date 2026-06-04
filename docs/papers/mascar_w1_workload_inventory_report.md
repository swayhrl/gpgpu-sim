# Mascar W1 Workload Inventory Report

## Goal

W1 builds a canonical inventory for the 30 Mascar paper Table III workload rows and audits what is currently available in the local workspace. This stage does not run full benchmark suites and does not modify Mascar simulator mechanisms.

## Paper Table III Target

The target is the paper's 30-row Table III workload set from Rodinia and Parboil. Rows with numeric suffixes are preserved as distinct paper rows because they may represent different kernels or phases.

## Canonical Workload List

Memory-intensive rows:

- BP-2
- bfs
- histo-1
- histo-3
- kmeans-1
- lbm
- leuko-1
- mrig-1
- mrig-2
- mummer
- particle
- sad-2
- spmv
- srad-1
- srad-2

Compute-intensive rows:

- BP-1
- cutcp
- histo-2
- histogram
- kmeans-2
- lavaMD
- leuko-2
- leuko-3
- mri-q
- mrig-3
- pathfinder
- sad-1
- sgemm
- stencil
- tpacf

The canonical machine-readable manifest is `experiments/paper-mascar/workloads/mascar_table_iii_workload_manifest.csv`.

## Local Search Roots

The audit script searched these roots:

- `/workspace/repos/gpgpu-workloads`
- `/workspace/repos/gpgpu-sim_simulations`
- `/workspace/repos/gpgpu-sim_distribution`
- `/workspace/repos`
- `/workspace`

The script skipped hidden VCS directories, build outputs, run logs, review packs, `node_modules`, and other noisy generated directories.

## Availability Summary

Audited result counts:

- `available`: 6
- `partial_phase_unknown`: 9
- `missing_binary`: 14
- `missing_source`: 1

Phase mapping counts:

- `approximate`: 6
- `app_only`: 9
- `not_found`: 15

The audited manifest is `experiments/paper-mascar/workloads/mascar_table_iii_workload_manifest_audited.csv`.

## Workloads Available Now

The following Table III rows have local tiny wrappers in `/workspace/repos/gpgpu-workloads` and are ready for W2 wrapper dry-runs or short smoke:

- `bfs` -> `rodinia_bfs`
- `spmv` -> `parboil_spmv`
- `mri-q` -> `parboil_mri_q`
- `pathfinder` -> `rodinia_pathfinder`
- `sgemm` -> `parboil_sgemm`
- `stencil` -> `parboil_stencil`

These are marked `approximate` rather than paper-identical because the local wrappers use small validation inputs rather than the original Table III experiment inputs.

## Workloads Requiring Build

Raw source exists locally but no bounded runnable wrapper or verified GPGPU-Sim binary was found for:

- `lbm`
- `leuko-1`
- `leuko-2`
- `leuko-3`
- `mrig-1`
- `mrig-2`
- `mrig-3`
- `mummer`
- `particle`
- `sad-1`
- `sad-2`
- `cutcp`
- `lavaMD`
- `tpacf`

These are marked `missing_binary`.

## Workloads Missing Binary Data Source

`histogram` remains `missing_source` because no clear local Table III source or wrapper mapping was identified. Candidate paths are recorded in `experiments/paper-mascar/workloads/audit/workload_candidate_paths.txt` for manual review.

No row was marked `missing_data` in W1 because rows without a runnable command were stopped earlier at source or binary availability.

## Phase Mapping Uncertainties

The following rows have local app-level commands but no verified mapping from the paper row to the local kernel or phase:

- BP-1
- BP-2
- histo-1
- histo-2
- histo-3
- kmeans-1
- kmeans-2
- srad-1
- srad-2

These rows remain distinct in the manifest and are marked `partial_phase_unknown`.

## Recommended W2 Actions

W2 should create one wrapper per paper row. Ready rows can call the local `gpgpu-workloads/scripts/run_one.sh` commands. Missing or phase-unknown rows should use placeholder wrappers that support `--dry-run`, `--print-command`, and `--help`, and return exit 77 for actual runs.

W2 should keep `rodinia_hotspot` outside the Table III manifest. It can remain as an extra smoke workload only.

## Limitations

This audit establishes workload infrastructure readiness, not paper-comparable performance coverage. It does not run full Rodinia or Parboil suites, does not prove paper input equivalence, and does not resolve kernel phase names for suffix rows.
