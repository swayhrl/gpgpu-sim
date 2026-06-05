# Mascar W17B Build and Binary Recovery

## Goal

Attempt local build or binary recovery for Table III rows marked missing_binary without downloading external source or data and without modifying Mascar mechanisms.

## Build Candidates Attempted

W17B attempted all 14 missing_binary Table III rows. Rows sharing the same raw source still retain independent result rows: leuko-1/2/3, mrig-1/2/3, sad-1/2.

Every high-priority source row received at least two attempts: native build plus fallback build or repeated fallback where no distinct fallback existed.

## Binaries Recovered

No valid ELF CUDA application binary was recovered for missing_binary rows after correcting binary detection to reject source/data files with executable bits.

## Build Failures and Reasons

- Parboil raw workloads (lbm, mri-gridding, sad, cutcp, tpacf) failed through direct src/cuda and fallback CUDA/base Makefile paths. The dominant blocker is raw Parboil benchmark layout without standalone local wrapper/target in these directories.
- Rodinia leukocyte reached link stage but failed on missing libcuda in /usr/local/cuda/lib64.
- Rodinia lavaMD failed due obsolete sm_13 architecture unsupported by CUDA 11.8 nvcc.
- Rodinia particlefilter also failed old CUDA architecture/toolchain compatibility.
- Rodinia mummergpu failed in native/fallback build due source compile errors under the current compiler/toolchain.

## Missing Data Issues

Only mummergpu has a clear local data directory. Most other raw missing-binary rows also lack bounded local tiny input data directories suitable for wrapper normalization.

## Updated Ready Candidates

W17B did not create new binary-backed ready rows. W17C can still promote app-level phase_unknown rows to ready with app_level_pending_kernel_trace because their binaries and app commands already exist.

## Workloads Still Blocked

The original 14 missing_binary rows remain blocked by build/toolchain or missing data/wrapper issues. histogram remains source-blocked.

## W17C and W18 Recommendations

- W17C should update phase_unknown rows to app-level ready, not exact phase ready.
- W18 should run kernel launch trace for BP, histo, kmeans, and srad suffix rows.
- Future build work should add proper benchmark-specific wrapper directories rather than compiling raw source trees in place.

## Build Logs

Build logs are outside git under: /workspace/tmp/mascar_w17_build_logs_20260605_135412
