# Mascar W15A Guidance: M3 Static Diagnostic Counters

## Stage position

This is W15A of Mascar workload coverage.

Current state:
- M1 L1 saturation probe implemented.
- M2 EP/MP owner scheduling implemented.
- M3 non-owner hit-only / miss-NACK implemented.
- M4 load-only re-execution queue implemented.
- W7/W8/W9-W14 show M2 and M4 can activate.
- M3 counters remain zero.

W15A must add diagnostic-only counters and trace points to determine why M3 is not activating.

## Goal

Determine whether M3 is blocked by:
- config off
- not in MP
- no owner
- current warp is owner
- instruction not load
- scheduler still blocks non-owner load
- LSU/L1D path not reached
- hit-only probe not called
- hit-only probe called but always not-hit
- collector not parsing M3 stats

## Hard constraints

1. All new diagnostic behavior must be gated by a new knob.
2. Default behavior must remain unchanged.
3. Do not change M3 active semantics unless a clear bug is found.
4. Do not remove existing M1-M4 stats.
5. Do not run full benchmark suites in W15A.
6. Do not fetch upstream.
7. Do not create a new branch.
8. Do not use git add . or git add -A.
9. Do not commit W15 outputs.

## New config knobs

Add near existing Mascar knobs:

- int gpgpu_mascar_enable_m3_diagnostic
- int gpgpu_mascar_m3_diag_verbose
- unsigned gpgpu_mascar_m3_diag_max_trace

Suggested defaults:
- gpgpu_mascar_enable_m3_diagnostic = 0
- gpgpu_mascar_m3_diag_verbose = 0
- gpgpu_mascar_m3_diag_max_trace = 64

Meaning:
- diagnostic enabled only when gpgpu_enable_mascar and gpgpu_mascar_enable_m3_diagnostic are both nonzero.
- verbose prints bounded trace lines.
- max_trace limits log spam.

## Required diagnostic counters

Print all counters as paper_mascar_m3diag_*.

Required counters:

- paper_mascar_m3diag_enabled
- paper_mascar_m3diag_mem_inst_seen
- paper_mascar_m3diag_load_inst_seen
- paper_mascar_m3diag_mp_mode_seen
- paper_mascar_m3diag_owner_valid_seen
- paper_mascar_m3diag_nonowner_load_candidate
- paper_mascar_m3diag_scheduler_allow_nonowner_load
- paper_mascar_m3diag_scheduler_block_nonowner_mem
- paper_mascar_m3diag_lsu_l1d_access_seen
- paper_mascar_m3diag_l1d_nonowner_load_seen
- paper_mascar_m3diag_hitonly_probe_called
- paper_mascar_m3diag_hitonly_probe_hit
- paper_mascar_m3diag_hitonly_probe_nack
- paper_mascar_m3diag_active_hitonly_access_called
- paper_mascar_m3diag_active_hitonly_access_hit
- paper_mascar_m3diag_active_hitonly_access_nack

Skip reason counters:

- paper_mascar_m3diag_skip_config_off
- paper_mascar_m3diag_skip_not_mp
- paper_mascar_m3diag_skip_no_owner
- paper_mascar_m3diag_skip_owner_warp
- paper_mascar_m3diag_skip_not_load
- paper_mascar_m3diag_skip_not_l1d
- paper_mascar_m3diag_skip_nonowner_hitonly_disabled
- paper_mascar_m3diag_skip_not_ready_or_scoreboard
- paper_mascar_m3diag_skip_m2_blocked_before_lsu

Optional trace counters:
- paper_mascar_m3diag_trace_lines_emitted

## Required instrumentation points

Instrument without changing behavior.

### Scheduler-side points

In or near M2/M3 scheduler gating helper:

Record:
- memory instruction seen
- load instruction seen
- MP mode active
- owner valid
- non-owner load candidate
- scheduler allowed non-owner load
- scheduler blocked non-owner memory

If a non-owner load should be allowed by M3 but is blocked by M2, increment:
- paper_mascar_m3diag_skip_m2_blocked_before_lsu

This is critical. It detects the most likely implementation bug.

### LSU/L1D-side points

In L1D memory access path:

Record:
- L1D access seen
- non-owner load seen
- hit-only probe called
- active hit-only access called
- probe hit/nack

If M3 diagnostic says scheduler allowed non-owner load but LSU never sees it, record that through counters if feasible.

### Config-side skip reasons

When M3 diagnostic sees a possible candidate but M3 is disabled due config, increment config skip counters.

## Bounded verbose trace

If gpgpu_mascar_m3_diag_verbose is enabled, print at most max_trace lines like:

paper_mascar_m3diag_trace cycle=... core=... warp=... stage=scheduler condition=nonowner_load_candidate mp=1 owner_valid=1 owner=...
paper_mascar_m3diag_trace cycle=... core=... warp=... stage=l1d probe=called status=HIT/NACK

Do not spam more than max_trace lines per kernel or per simulation.

## Diagnostic config

Add:

- configs/hrl-repro/SM7_QV100_mascar_m3diag_on/

Start from M4 active config.

Required settings:
- -gpgpu_enable_mascar 1
- -gpgpu_mascar_enable_l1_saturation_probe 1
- -gpgpu_mascar_enable_mp_owner_telemetry 1
- -gpgpu_mascar_enable_mp_owner_scheduling 1
- -gpgpu_mascar_enable_nonowner_hit_only_probe 1
- -gpgpu_mascar_enable_nonowner_hit_only 1
- -gpgpu_mascar_enable_reexec_queue_probe 1
- -gpgpu_mascar_enable_reexec_queue 1
- -gpgpu_mascar_enable_m3_diagnostic 1
- -gpgpu_mascar_m3_diag_verbose 1
- -gpgpu_mascar_m3_diag_max_trace 128
- -gpgpu_mascar_enable_scheduling 0
- -gpgpu_mascar_enable_would_deprioritize 0

Also add a forced-MP diagnostic config:

- configs/hrl-repro/SM7_QV100_mascar_m3diag_forced_mp_on/

It should be identical but use aggressive saturation settings:
- larger gpgpu_mascar_l1_saturation_margin
- larger gpgpu_mascar_l1_saturation_recent_window

Do not use forced-MP config for performance claims. It is diagnostic only.

## Collector update

Update common collector to parse paper_mascar_m3diag_* counters.

Required:
- results CSV should include all key m3diag fields or at least a m3diag_* subset.
- summary should report skip reason counts.

## Documentation

Create:

- docs/papers/mascar_w15a_m3_static_diagnostic.md

Required sections:
1. Goal
2. Why M3 is hard to trigger
3. Diagnostic counters
4. Scheduler instrumentation
5. LSU/L1D instrumentation
6. Configs
7. How to interpret skip reasons
8. Safety and default-off behavior

## Validation

Run:

- git diff --check
- source setup_environment release && make -j2
- grep gpgpu_mascar_enable_m3_diagnostic
- grep paper_mascar_m3diag_
- python3 -m py_compile experiments/common/gpgpusim_matrix/collect_gpgpusim_stats.py

Do not proceed to W15B until build passes.

## Stop conditions

Stop only if:
1. build cannot be restored.
2. instrumentation requires broad M1-M4 redesign.
3. diagnostic code changes M3 behavior by default.
4. elapsed time exceeds 90 minutes before W15B.
