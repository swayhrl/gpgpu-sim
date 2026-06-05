# Mascar W17 Workload Availability Expansion Report

## Executive Summary

W17 expanded Table III app-level availability from 6 ready rows to 15 ready rows. The 9 newly ready rows are app-level only and are marked app_level_pending_kernel_trace; W17 does not claim exact paper phase mapping.

## Starting Coverage Before W17

- Table III rows: 30
- ready rows before W17: 6
- placeholder phase_unknown rows before W17: 9
- missing_binary rows before W17: 14
- missing_source rows before W17: 1

## W17A Audit Results

W17A generated a 30-row gap matrix and candidate inventory. Local source exists for all missing_binary rows. histogram remains missing_source.

## W17B Build/Binary Recovery Results

W17B attempted native and fallback builds for all 14 missing_binary rows. No valid ELF CUDA binary was recovered. Build blockers include raw Parboil Makefile target issues, missing libcuda link dependency, old CUDA architecture flags, and current compiler incompatibilities.

## W17C Wrapper Normalization Results

W17C generated an updated 30-row command manifest and promoted BP, histo, kmeans, and srad phase rows to app-level ready. _run_table_iii_common.sh was fixed to pass GPGPUSIM_CONFIG_OVERRIDE so actual smoke uses the requested config override.

## Final Ready/Unavailable Counts

- ready rows after W17: 15
- unavailable rows after W17: 15
- phase pending rows after W17: 9

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

## Still Unavailable Workloads and Reasons

- lbm: missing_binary after native/fallback Parboil build attempts.
- leuko_1/leuko_2/leuko_3: missing_binary; link fails on missing libcuda.
- mrig_1/mrig_2/mrig_3: missing_binary after native/fallback Parboil build attempts.
- mummer: missing_binary; current compiler/source build failure.
- particle: missing_binary; old CUDA/toolchain incompatibility.
- sad_1/sad_2: missing_binary after native/fallback Parboil build attempts.
- cutcp: missing_binary after native/fallback Parboil build attempts.
- lavaMD: missing_binary; obsolete sm_13 CUDA architecture.
- tpacf: missing_binary after native/fallback Parboil build attempts.
- histogram: missing_source.

## Phase Mapping Caveats

The newly ready phase rows are app-level commands only. Exact kernel/phase mapping requires W18 kernel launch trace.

## W18 Kernel Launch Trace Handoff

Use w17_next_action_for_W18.csv for BP, histo, kmeans, and srad phase rows. W18 should map paper suffix rows to local kernel launch index/name.

## Raw Log/Archive Note

Raw build and smoke logs were archived to: /workspace/tmp/mascar_w17_raw_logs_20260605_135428.tar.gz

## Limitations

This is workload availability expansion, not paper-comparable performance reproduction. Tiny local inputs remain smoke inputs.

supplemental_repo_raw_logs=/workspace/tmp/mascar_w17_repo_raw_logs_20260605_135502.tar.gz
