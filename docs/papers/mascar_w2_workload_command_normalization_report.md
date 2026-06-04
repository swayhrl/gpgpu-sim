# Mascar W2 Workload Command Normalization Report

## Goal

W2 turns the W1 Table III inventory into normalized command metadata and one wrapper per paper row. It does not run a full benchmark sweep and does not modify Mascar simulator mechanisms.

## Inputs From W1

W2 used:

- `experiments/paper-mascar/workloads/mascar_table_iii_workload_manifest.csv`
- `experiments/paper-mascar/workloads/mascar_table_iii_workload_manifest_audited.csv`
- `experiments/paper-mascar/workloads/mascar_workload_availability_summary.csv`
- `/workspace/repos/gpgpu-workloads/manifests/workload_manifest.csv`

The audited inventory has all 30 Table III rows.

## Wrapper Convention

Each paper row has a wrapper at `experiments/paper-mascar/workloads/wrappers/run_<paper_id>.sh`.

Every wrapper supports:

- `--help`
- `--dry-run`
- `--print-command`

Actual unavailable rows exit 77. Ready wrappers run through the local `/workspace/repos/gpgpu-workloads/scripts/run_one.sh` path and accept:

- `MASCAR_RUN_DIR`
- `MASCAR_CONFIG_DIR`
- `MASCAR_TIMEOUT_SEC`
- `GPGPUSIM_ROOT`
- `GPGPU_WORKLOAD_ROOT`

## Available Ready Workloads

Ready wrappers:

- `run_bfs.sh`
- `run_spmv.sh`
- `run_mri_q.sh`
- `run_pathfinder.sh`
- `run_sgemm.sh`
- `run_stencil.sh`

These are local tiny-input wrappers and should be treated as availability smoke, not paper-equivalent runs.

## Placeholder Workloads

Placeholder wrappers:

- 9 `placeholder_phase_unknown` rows for app-level commands without resolved paper phase mapping.
- 14 `placeholder_missing_binary` rows where raw source exists but no local runnable wrapper or binary is available.
- 1 `placeholder_missing_source` row for `histogram`.

Placeholder wrappers pass dry-run and print the reason, but actual runs exit 77.

## Build Helper

`experiments/paper-mascar/workloads/build_available_workloads.sh` reads the command manifest and builds only rows marked `build_required=yes` with a known build command. W2 has no known bounded build command rows, so the helper exits 0 and reports no buildable workloads.

## Smoke Helper

`experiments/paper-mascar/workloads/run_available_workload_smoke.sh` dry-runs all 30 wrappers and optionally runs actual ready wrappers with `RUN_ACTUAL=1`.

W2 validation ran dry-run smoke for all 30 wrappers. It also ran one bounded actual smoke for `stencil` with a 300 second timeout.

## Extra Smoke Workloads

`rodinia_hotspot` is recorded in `experiments/paper-mascar/workloads/extra_smoke_workloads.csv` as `extra_not_table_iii`. It is not included in the canonical Table III manifest.

## How W3 W4 Should Use This Infrastructure

W3/W4 can use `mascar_table_iii_command_manifest.csv` as the workload command source. It should:

- Run only rows with `wrapper_status=ready` unless a missing wrapper is resolved.
- Keep phase-unknown rows separate until the paper kernel or phase mapping is identified.
- Use `--dry-run` as a cheap preflight before any actual run.
- Use `MASCAR_CONFIG_DIR` to inject baseline and Mascar configs without editing workload directories.
- Apply a timeout to every actual run.

## Remaining Gaps To Full Table III Coverage

Remaining work:

- Build or import runnable wrappers and binaries for the 14 raw-source-only rows.
- Locate or identify the correct `histogram` source.
- Resolve paper phase mappings for BP, histo, kmeans, and srad suffix rows.
- Replace tiny local smoke inputs with paper-comparable inputs before making performance claims.
