# Mascar M2B Active Owner Scheduling

## Scope

M2B turns the M2A control plane into active MP owner-warp scheduling. Behavior
changes are gated by `gpgpu_enable_mascar=1` and
`gpgpu_mascar_enable_mp_owner_scheduling=1`; baseline and M2A telemetry configs
remain scheduling-safe.

## Active MP Condition

Active scheduling is enabled only through the new M2B knob:
`src/gpgpu-sim/shader.h:2512`. MP mode itself is derived from the recent L1D
saturation flag in `mascar_m2_cycle_begin()`:
`src/gpgpu-sim/shader.cc:2335`. Saturation clear returns to EP mode and releases
the owner: `src/gpgpu-sim/shader.cc:2339`.

## Scheduler Integration

The scheduler keeps its existing `order_warps()` call, updates M2 owner state,
then applies M2 priority ordering: `src/gpgpu-sim/shader.cc:1304`. The helper is
declared in `src/gpgpu-sim/shader.h:490` and implemented at
`src/gpgpu-sim/shader.cc:1678`.

`apply_mascar_m2_priority_ordering()` returns immediately unless active
scheduling, compute-first, and MP mode are all enabled:
`src/gpgpu-sim/shader.cc:1678`. It classifies only ready, non-scoreboard-blocked
warps, preserving uncertain entries in a tail group:
`src/gpgpu-sim/shader.cc:1701`. The final order is compute-ready,
owner-memory-ready, first-memory-ready, non-owner-memory-ready, and tail:
`src/gpgpu-sim/shader.cc:1734`. Reorder and candidate counters are updated via
`mascar_m2_note_priority_reorder()`:
`src/gpgpu-sim/shader.cc:2435`.

## Owner Acquire, Issue, and Release

Owner acquire records the owner warp and cycle in shared `shader_core_ctx` state:
`src/gpgpu-sim/shader.cc:2286`. Owner memory issue success updates last progress
and `owner_mem_issue`: `src/gpgpu-sim/shader.cc:2412`. Owner compute issue is
counted separately: `src/gpgpu-sim/shader.cc:2420`.

Owner release reasons implemented in M2B are saturation clear, scoreboard block,
warp done, max hold, and no progress:
`src/gpgpu-sim/shader.cc:2297`. Max-hold and no-progress releases also increment
the deadlock-force-release stat: `src/gpgpu-sim/shader.cc:2312`.

## Non-Owner Memory Blocking

The M2B active gate is inserted in the memory instruction path after the old
proxy Mascar gate and before `m_mem_out->has_free()`:
`src/gpgpu-sim/shader.cc:1437` through `src/gpgpu-sim/shader.cc:1445`.
`mascar_m2_should_block_memory_warp()` returns false unless active scheduling,
MP mode, and a memory instruction are present:
`src/gpgpu-sim/shader.cc:2443`. It acquires an owner if none exists, allows the
owner, and blocks non-owner memory warps while setting the WST stall bit:
`src/gpgpu-sim/shader.cc:2448` through `src/gpgpu-sim/shader.cc:2459`.

This is a scheduler-level block only. It does not call `issue_warp`, does not
call `m_mem_out->has_free()` for the blocked instruction, and does not change
cache request status or cache access return behavior.

## Stats

Active stats are printed as `paper_mascar_m2_*`, including
`paper_mascar_m2_nonowner_mem_block`,
`paper_mascar_m2_owner_mem_issue`,
`paper_mascar_m2_owner_compute_issue`,
`paper_mascar_m2_compute_priority_reorder`, and deadlock guard counters:
`src/gpgpu-sim/gpu-sim.cc:1949` through `src/gpgpu-sim/gpu-sim.cc:1966`.

## Config

`configs/hrl-repro/SM7_QV100_mascar_m2_owner_sched_on/` enables the M1 L1D probe,
M2 telemetry, and new M2 owner scheduling, while leaving old proxy scheduling and
would-deprioritize off:
`configs/hrl-repro/SM7_QV100_mascar_m2_owner_sched_on/gpgpusim.config:241`
through
`configs/hrl-repro/SM7_QV100_mascar_m2_owner_sched_on/gpgpusim.config:248`.

## Remaining Gaps for M3/M4

M2B does not implement non-owner hit-only access, miss-NACK, cache access
re-execution, a re-execution queue, or one-memory-instruction-per-warp
re-execution enforcement. Those are intentionally left for later stages.
