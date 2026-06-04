# Mascar M3A Non-owner Hit-only Probe

## Goal

M3A adds passive telemetry for the paper's non-owner L1 hit-only idea. It
observes whether a non-owner load in MP mode would hit in L1D or would receive a
NACK, without changing scheduler behavior or cache behavior.

## Paper Mechanism Mapping

The paper allows non-owner memory requests that are already near L1 to complete
only if they hit in L1. M3A maps this to a read-only L1D tag probe. Only `HIT`
is counted as would-hit; all other statuses are would-NACK.

## Cache-side Helper Semantics

The new cache API is declared in `src/gpgpu-sim/gpu-cache.h:1606`.
`data_cache::mascar_l1_hit_only_probe()` computes the block address and calls
`tag_array::probe(..., probe_mode=true)` only:
`src/gpgpu-sim/gpu-cache.cc:2082`. It does not call `process_tag_probe()`, so it
does not update LRU, stats, bandwidth, MSHR state, miss queue state, or events.

## Passive Sampling Points

The ldst_unit helper checks the M3A candidate conditions and then calls the
cache-side probe: `src/gpgpu-sim/shader.cc:2226`. It is inserted before normal
L1D access in the no-latency L1D path and the L1 latency queue path:
`src/gpgpu-sim/shader.cc:2895` and `src/gpgpu-sim/shader.cc:2925`.

## New Knobs

The new M3 knobs are registered with defaults that keep behavior off:
`src/gpgpu-sim/gpu-sim.cc:795` through `src/gpgpu-sim/gpu-sim.cc:808`.
They are stored in `shader_core_config` at `src/gpgpu-sim/shader.h:1995`
through `src/gpgpu-sim/shader.h:2004`.

## New Stats

M3 stats live in `mascar_m3_stats`: `src/gpgpu-sim/shader.h:431` through
`src/gpgpu-sim/shader.h:452`. Runtime output prints the passive
`paper_mascar_m3_probe_*` counters in `src/gpgpu-sim/gpu-sim.cc:1987` through
`src/gpgpu-sim/gpu-sim.cc:2015`.

## HIT_RESERVED Handling

`HIT_RESERVED` is not treated as usable data in M3A. The probe result is counted
as a conservative NACK together with `RESERVATION_FAIL` and `MSHR_HIT`:
`src/gpgpu-sim/shader.cc:2565`. This matches the M3 guidance to avoid treating
reserved data as a complete L1 hit.

## Behavior Safety

M3A is behavior-safe because the only cache-side action is a read-only tag
probe, and because normal L1D access continues immediately afterward. The M3A
config keeps owner scheduling off and active hit-only off:
`configs/hrl-repro/SM7_QV100_mascar_m3_hitonly_probe_on/gpgpusim.config:248`
through
`configs/hrl-repro/SM7_QV100_mascar_m3_hitonly_probe_on/gpgpusim.config:251`.

## M3B Plan

M3B uses the same hit-only probe as an active guard: non-owner loads may reach
L1D in MP mode, L1 hits complete normally, and non-hits are converted to local
NACK/retry without sending an L2 request.
