# Mascar M2A EP/MP Owner Telemetry

## Scope

M2A adds a passive EP/MP owner-warp control plane on top of the M1 L1D
saturation probe. It does not change scheduler issue order and does not change
cache access return behavior.

## Knobs

- `gpgpu_mascar_enable_mp_owner_telemetry` enables passive M2 owner telemetry:
  `src/gpgpu-sim/gpu-sim.cc:783`.
- `gpgpu_mascar_enable_mp_owner_scheduling` is registered but defaults off:
  `src/gpgpu-sim/gpu-sim.cc:787`.
- `gpgpu_mascar_m2_compute_first`,
  `gpgpu_mascar_l1_saturation_recent_window`,
  `gpgpu_mascar_owner_max_hold_cycles`, and
  `gpgpu_mascar_owner_no_progress_limit` are config fields:
  `src/gpgpu-sim/shader.h:1928`,
  `src/gpgpu-sim/shader.h:1932`,
  `src/gpgpu-sim/shader.h:1933`,
  `src/gpgpu-sim/shader.h:1934`.
- Telemetry is gated by `gpgpu_enable_mascar` and either M2 telemetry or M2
  scheduling: `src/gpgpu-sim/shader.h:2507`.

## L1 Recent Saturation Flag

M1's raw L1D pressure samples now feed a recent saturation flag. The ldst_unit
stores the recent flag and timestamps in `src/gpgpu-sim/shader.h:1633`.
Sampling sets or clears the flag from MSHR/miss-queue pressure in
`src/gpgpu-sim/shader.cc:2196`, updates its timestamp in
`src/gpgpu-sim/shader.cc:2204`, and reservation failures force it true in
`src/gpgpu-sim/shader.cc:2213`. The recent-window query returns false when the
probe is disabled or stale: `src/gpgpu-sim/shader.cc:2246`.

## Per-SM Owner Control Plane

Owner state lives in `shader_core_ctx`, so schedulers on the same SM share it:
`src/gpgpu-sim/shader.h:3086`. The shared state includes MP mode, owner valid
bit, owner warp id, owner acquire cycle, last owner memory-issue cycle, WST
memory/stall approximations, and `mascar_m2_stats`:
`src/gpgpu-sim/shader.h:3086` through `src/gpgpu-sim/shader.h:3094`.

Each scheduler cycle calls `mascar_m2_cycle_begin()` after the existing scheduler
ordering step: `src/gpgpu-sim/shader.cc:1304`. That method derives MP mode from
the recent L1 saturation flag, counts EP/MP cycles, releases owner on saturation
clear, and applies owner done/max-hold/no-progress guards:
`src/gpgpu-sim/shader.cc:2325`.

## Passive WST/WRC Approximation

M2A approximates WST with per-warp memory and stall bits. Memory instructions
set the WST memory bit and increment `wst_mem_bit_set`:
`src/gpgpu-sim/shader.cc:2376`. Scoreboard-blocked candidates set the stall bit:
`src/gpgpu-sim/shader.cc:2381`. Owner release on scoreboard block approximates
dependency-based owner release: `src/gpgpu-sim/shader.cc:2426`.

WRC is represented by the shared owner state and ready-candidate accounting.
Passive candidate observation increments memory/compute-ready counters in MP,
records would-block/would-owner/would-compute-first telemetry, and can acquire a
telemetry owner without affecting issue order: `src/gpgpu-sim/shader.cc:2388`.

## Stats

M2 stats live in `mascar_m2_stats`: `src/gpgpu-sim/shader.h:360` through
`src/gpgpu-sim/shader.h:382`. They are printed with `paper_mascar_m2_*` names:
`src/gpgpu-sim/gpu-sim.cc:1901` through `src/gpgpu-sim/gpu-sim.cc:1966`.

## Config

`configs/hrl-repro/SM7_QV100_mascar_m2_owner_telemetry_on/` enables the M1 L1D
probe and M2 owner telemetry but keeps both old proxy scheduling and new owner
scheduling off: `configs/hrl-repro/SM7_QV100_mascar_m2_owner_telemetry_on/gpgpusim.config:241`
through
`configs/hrl-repro/SM7_QV100_mascar_m2_owner_telemetry_on/gpgpusim.config:248`.

## Explicit Non-Goals

M2A does not implement active owner scheduling, non-owner hit-only behavior,
miss-NACK, cache access re-execution, or any cache access return change.
