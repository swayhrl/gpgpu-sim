# Mascar W15A M3 Static Diagnostic

## Goal

W15A adds default-off diagnostics for the M3 non-owner hit-only path. The goal is to distinguish config/wrapper/input issues from an implementation path issue when M3 active counters remain zero.

## Why M3 is hard to trigger

M3 needs all of these to align: Mascar enabled, MP mode active, a valid owner warp, a different warp issuing a load, and an L1D hit-only probe that reaches the LSU/L1D path. Any missing condition makes M3 look inactive even when M2/M4 telemetry is alive.

## Diagnostic counters

All new counters are printed as `paper_mascar_m3diag_*`. They record scheduler visibility, skip reasons, LSU/L1D visibility, passive hit-only probe calls, active hit-only access calls, and bounded trace-line count.

## Scheduler instrumentation

Scheduler-side diagnostics count memory/load instructions, MP and owner visibility, non-owner load candidates, scheduler allows, scheduler blocks, and the critical `skip_m2_blocked_before_lsu` condition. That counter identifies a possible M3 implementation-path bug where M2 blocks a non-owner load before LSU can try hit-only access.

## LSU/L1D instrumentation

LSU-side diagnostics count every L1D access seen by the M3 diagnostic, non-owner load accesses in MP, passive hit-only probe calls, and active hit-only calls/results. This separates “scheduler allowed” from “LSU path reached”.

## Configs

Two diagnostic configs were added:

- `configs/hrl-repro/SM7_QV100_mascar_m3diag_on`
- `configs/hrl-repro/SM7_QV100_mascar_m3diag_forced_mp_on`

The forced-MP config uses aggressive L1 saturation margin/window and is diagnostic only, not a performance configuration.

## How to interpret skip reasons

- `skip_config_off`: diagnostic saw a possible path but the relevant M3/M2 active config was off.
- `skip_not_mp`: MP mode was not active.
- `skip_no_owner`: no owner warp was valid.
- `skip_owner_warp`: the current warp was the owner, so it is not an M3 non-owner candidate.
- `skip_not_load`: instruction was not a supported load.
- `skip_not_l1d`: LSU/L1D path was unavailable.
- `skip_nonowner_hitonly_disabled`: non-owner hit-only was not enabled.
- `skip_not_ready_or_scoreboard`: scheduler saw a scoreboard/not-ready block.
- `skip_m2_blocked_before_lsu`: M2 blocked a non-owner load before LSU could call M3.

## Safety and default-off behavior

The new behavior is gated by `gpgpu_enable_mascar && gpgpu_mascar_enable_m3_diagnostic`. Default values keep diagnostics and trace output disabled. Existing M1-M4 counters and active semantics are preserved.
