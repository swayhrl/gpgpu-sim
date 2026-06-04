# Mascar M3B Active Hit-only NACK

## Goal

M3B implements the active non-owner L1 hit-only / miss-NACK semantics after M2
owner scheduling. It does not implement the M4 re-execution queue.

## Paper Mechanism Implemented

In MP mode, non-owner loads can reach L1D only when
`gpgpu_mascar_enable_nonowner_hit_only=1`. If the L1D probe reports `HIT`, the
load completes through the normal read-hit path. If the probe reports
`MISS`, `SECTOR_MISS`, `HIT_RESERVED`, `RESERVATION_FAIL`, or `MSHR_HIT`, the
request is NACKed locally and no L2 request is sent.

## Scheduler-side Change

The scheduler still calls the M2 memory gate before `m_mem_out->has_free()`:
`src/gpgpu-sim/shader.cc:1439` through `src/gpgpu-sim/shader.cc:1447`.
Inside that M2 gate, M3 can allow a non-owner load to issue to LSU:
`src/gpgpu-sim/shader.cc:2494` through `src/gpgpu-sim/shader.cc:2506`.
The M3 helper allows only load candidates and records non-load blocks:
`src/gpgpu-sim/shader.cc:2574`.

Stores, atomics, and non-load memory operations remain blocked by M2 by default,
because `mascar_m3_inst_is_load_candidate()` rejects them:
`src/gpgpu-sim/shader.cc:2521`.

## Cache-side Hit-only Access

`data_cache::mascar_l1_access_hit_only()` first probes with
`probe_mode=true`, rejects writes/atomics, and immediately returns
`RESERVATION_FAIL` unless the probe is `HIT`:
`src/gpgpu-sim/gpu-cache.cc:2090` through
`src/gpgpu-sim/gpu-cache.cc:2100`. Only after a confirmed hit does it call
`process_tag_probe(false, HIT, ...)`, which runs the existing read-hit path:
`src/gpgpu-sim/gpu-cache.cc:2102`.

This avoids the normal read-miss path and therefore avoids `send_read_request()`
for active M3 non-owner misses.

## NACK Semantics

The ldst_unit wrapper calls active hit-only only when the M3 active conditions
hold. Non-hit probe results set `m3_nack=true`, update M3 NACK stats, and return
`RESERVATION_FAIL` as a local retry signal:
`src/gpgpu-sim/shader.cc:2237` through `src/gpgpu-sim/shader.cc:2268`.

M3 NACK is not counted as a real L1 reservation failure. Both L1D paths call
`mascar_note_l1_reservation_fail()` only when `m3_nack` is false:
`src/gpgpu-sim/shader.cc:2901` and `src/gpgpu-sim/shader.cc:2929`.

## L1 Latency Queue Handling

The L1 latency queue path uses the same `mascar_m3_l1d_access()` wrapper:
`src/gpgpu-sim/shader.cc:2923`. On an M3 NACK the status is
`RESERVATION_FAIL`, so the existing latency-queue code leaves the mem_fetch in
the queue for retry instead of deleting it or sending it to L2.

The no-latency path also uses the wrapper before `process_cache_access()`:
`src/gpgpu-sim/shader.cc:2898`. In that path, `RESERVATION_FAIL` leaves the
instruction access queue intact and deletes only the transient mem_fetch, which
matches the guidance for local retry before M4.

## NACK Guard

M3 tracks repeated non-owner NACKs per warp. When the configured threshold is
reached, it clears the current owner and increments
`paper_mascar_m3_nack_guard_owner_release`:
`src/gpgpu-sim/shader.cc:2623` through `src/gpgpu-sim/shader.cc:2635`.
This guard does not allow a non-owner miss to proceed to L2.

## New Knobs

M3B uses the M3 knobs registered in `src/gpgpu-sim/gpu-sim.cc:795` through
`src/gpgpu-sim/gpu-sim.cc:808` plus the NACK guard threshold registered at
`src/gpgpu-sim/gpu-sim.cc:832`. Active hit-only additionally requires M2 owner
scheduling, as encoded in `mascar_m3_active_hit_only_enabled()`:
`src/gpgpu-sim/shader.h:2611`.

## New Stats

Active M3 stats are printed as `paper_mascar_m3_*`:
`src/gpgpu-sim/gpu-sim.cc:2016` through `src/gpgpu-sim/gpu-sim.cc:2038`.
They cover scheduler LSU allowance, non-load blocks, hit-only attempts, hits,
NACK classes, owner/MP bypasses, and the NACK guard threshold.

## Configs

The passive M3A config keeps active hit-only off:
`configs/hrl-repro/SM7_QV100_mascar_m3_hitonly_probe_on/gpgpusim.config:250`
through
`configs/hrl-repro/SM7_QV100_mascar_m3_hitonly_probe_on/gpgpusim.config:253`.

The active M3B config enables M2 owner scheduling and M3 active hit-only, with
old proxy scheduling still off:
`configs/hrl-repro/SM7_QV100_mascar_m3_hitonly_nack_on/gpgpusim.config:243`
through
`configs/hrl-repro/SM7_QV100_mascar_m3_hitonly_nack_on/gpgpusim.config:253`.

## Validation

- `git diff --check`: pass.
- `source setup_environment release && make -j2`: pass.
- Smoke run skipped because no obvious short benchmark environment was selected
  and full benchmarks are disallowed.

## Remaining Gap for M4

M3B does not implement the re-execution queue, request recycling queue, or
one-memory-instruction-per-warp re-execution enforcement. NACKed requests use
the existing local retry behavior until M4 adds a queue that lets the LSU make
progress on other work.
