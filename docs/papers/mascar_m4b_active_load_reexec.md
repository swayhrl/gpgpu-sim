# Mascar M4B Active Load Re-Execution Queue

M4B enables load-only cache access re-execution under
`gpgpu_mascar_enable_reexec_queue=1`.

## Implemented

- Active gating requires `gpgpu_enable_mascar` and
  `gpgpu_mascar_enable_reexec_queue` at `src/gpgpu-sim/shader.cc:2324`.
- Queue entries own the `mem_fetch *` while queued. Initial enqueue constructs
  the entry and sets the per-warp bit at `src/gpgpu-sim/shader.cc:2390` through
  `src/gpgpu-sim/shader.cc:2428`.
- One entry per warp is enforced through `mascar_m4_warp_has_entry` at
  `src/gpgpu-sim/shader.cc:2345` and checked before enqueue at
  `src/gpgpu-sim/shader.cc:2405`.
- Queue-full or same-warp in-queue load dispatch stalls new L1D load work at
  `src/gpgpu-sim/shader.cc:3081` through `src/gpgpu-sim/shader.cc:3085`.
- No-latency L1D path enqueues on M3 NACK or real `RESERVATION_FAIL` at
  `src/gpgpu-sim/shader.cc:3137` through `src/gpgpu-sim/shader.cc:3147`.
- L1 latency queue path moves `mf_next` into the queue and frees the slot at
  `src/gpgpu-sim/shader.cc:3163` through `src/gpgpu-sim/shader.cc:3180`.
- Retry is run from `ldst_unit::cycle` after L1D/latency queue service and
  before new dispatch memory work at `src/gpgpu-sim/shader.cc:3997` through
  `src/gpgpu-sim/shader.cc:4001`.
- Non-owner MP retries reuse the M3 hit-only/NACK wrapper at
  `src/gpgpu-sim/shader.cc:2490` through `src/gpgpu-sim/shader.cc:2495`.
- HIT retry completion releases pending writes, scoreboard entries, depbar
  state, and calls `warp_inst_complete` at `src/gpgpu-sim/shader.cc:2430`
  through `src/gpgpu-sim/shader.cc:2463`.
- MISS/HIT_RESERVED retry exits the queue without deleting `mf`, letting the
  cache own it at `src/gpgpu-sim/shader.cc:2507` through
  `src/gpgpu-sim/shader.cc:2511`.
- RESERVATION_FAIL or M3 NACK retries requeue at tail at
  `src/gpgpu-sim/shader.cc:2512` through `src/gpgpu-sim/shader.cc:2533`.
- Queue-head owner takeover is exposed through
  `src/gpgpu-sim/shader.cc:2865` through `src/gpgpu-sim/shader.cc:2871` and
  invoked at `src/gpgpu-sim/shader.cc:2486`.

## Config

`configs/hrl-repro/SM7_QV100_mascar_m4_reexec_load_on/` enables active M4
load re-execution, keeps M3 hit-only/NACK active, and keeps old proxy Mascar
scheduling off.

## Ownership Summary

- Enqueue success: queue owns `mf`; original access queue entry or latency slot
  is removed.
- Retry HIT: queue completes the load and deletes `mf` if no write event was
  sent.
- Retry MISS/HIT_RESERVED: cache owns `mf`; queue clears the per-warp bit and
  does not delete.
- Retry RESERVATION_FAIL or NACK: queue keeps ownership and rotates the entry to
  the tail.
