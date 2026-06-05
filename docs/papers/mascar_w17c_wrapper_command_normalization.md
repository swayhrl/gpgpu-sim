# Mascar W17C Wrapper Command Normalization

## Goal

Normalize W17 Table III command metadata so locally runnable app-level phase rows can be used in smoke/matrix infrastructure without claiming exact paper phase mapping.

## Wrapper Updates Made

- Created `w17_command_manifest_updated.csv` with all 30 Table III rows preserved.
- Promoted BP-1/BP-2, histo-1/2/3, kmeans-1/2, and srad-1/2 from placeholder phase_unknown to app-level ready.
- Marked those promoted rows as `app_level_pending_kernel_trace`.
- Fixed `_run_table_iii_common.sh` to export `GPGPUSIM_CONFIG_OVERRIDE` when `MASCAR_CONFIG_DIR` is provided. This is required because `gpgpu-workloads/scripts/run_one.sh` only copies override configs into benchmark exec directories when that variable is set.

## Newly Ready Workloads

- bp_1
- bp_2
- histo_1
- histo_2
- histo_3
- kmeans_1
- kmeans_2
- srad_1
- srad_2

## App-Level Phase Pending Workloads

The newly ready rows are app-level only. Exact paper phase mapping requires W18 kernel launch trace.

## Still Unavailable Workloads

The 14 missing_binary rows remain unavailable after W17B build attempts. `histogram` remains missing_source.

## Smoke Results

- Dry-run smoke: 30/30 wrappers passed.
- Actual smoke: 9/9 app-level promoted rows passed after fixing config override propagation.

## W18 Dependencies

W18 should trace kernel launches and map paper phase rows to concrete local kernels for BP, histo, kmeans, and srad.

## Risks and Limitations

- App-level ready is not paper phase exact.
- Tiny local inputs are availability smoke inputs, not paper-comparable inputs.
- Missing-binary rows require toolchain/build-system work beyond W17.
