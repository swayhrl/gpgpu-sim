# Mascar M1 L1 Saturation Probe

## 1. Goal

M1 adds a passive L1D-side saturation probe for Mascar. The M0 implementation used a scheduler-side `m_mem_out` stall-streak proxy. This round adds read-only visibility into L1D MSHR and miss queue pressure so M2 can use a closer-to-paper saturation flag for EP/MP mode decisions.

M1 does not change scheduling, cache accept/reject behavior, or memory replay behavior.

## 2. Code changes

- Added read-only MSHR occupancy helpers:
  - `mshr_table::entries_used()`
  - `mshr_table::entries_limit()`
- Added `baseline_cache::mascar_l1_saturation_snapshot(...)`, which reads:
  - per-address MSHR fullness via `m_mshrs.full(mshr_addr)`
  - aggregate MSHR table occupancy
  - miss queue occupancy and limit
- Added passive LSU counters in `ldst_unit`, sampled only when both:
  - `gpgpu_enable_mascar != 0`
  - `gpgpu_mascar_enable_l1_saturation_probe != 0`
- Sampling occurs before existing L1D access attempts:
  - direct L1D `cache->access()` path in `ldst_unit::process_memory_access_queue_l1cache()`
  - latency-queue L1D `m_L1D->access()` path in `ldst_unit::L1_latency_queue_cycle()`
- Existing `RESERVATION_FAIL` handling is unchanged; M1 only increments a passive counter when that status is already returned.
- Added aggregation from `ldst_unit` to `shader_core_ctx`, `simt_core_cluster`, and `gpgpu_sim::gpu_print_stat()`.

## 3. New knobs

- `-gpgpu_mascar_enable_l1_saturation_probe`
  - default: `0`
  - enables passive L1D saturation sampling only when `gpgpu_enable_mascar=1`
- `-gpgpu_mascar_l1_saturation_margin`
  - default: `1`
  - almost-full margin; a structure is considered almost full when `used + margin >= limit`

The default-off path is baseline-safe: with `gpgpu_enable_mascar=0` or `gpgpu_mascar_enable_l1_saturation_probe=0`, the new sampling helpers return before touching counters.

## 4. New stats

Required stats:

- `paper_mascar_l1_sat_probe_enabled`
- `paper_mascar_l1_sat_sample`
- `paper_mascar_l1_sat_sample_saturated`
- `paper_mascar_l1_sat_mshr_full`
- `paper_mascar_l1_sat_mshr_almost_full`
- `paper_mascar_l1_sat_missq_full`
- `paper_mascar_l1_sat_missq_almost_full`
- `paper_mascar_l1_sat_reservation_fail`

Additional occupancy stats:

- `paper_mascar_l1_sat_mshr_used_sum`
- `paper_mascar_l1_sat_mshr_used_max`
- `paper_mascar_l1_sat_missq_used_sum`
- `paper_mascar_l1_sat_missq_used_max`

## 5. Why this is closer to paper Mascar than the M0 proxy

The M0 proxy inferred saturation from scheduler-side inability to issue a memory instruction into `m_mem_out`. That is useful as a coarse symptom, but it is not the L1-side pressure signal described by the paper.

M1 samples the L1D structures that can actually create memory-side back-pressure:

- MSHR full for the current block address
- aggregate MSHR entries almost full
- miss queue full
- miss queue almost full

This is still passive telemetry, but it gives M2 a direct L1-side saturation flag source instead of relying only on scheduler-side stall streaks.

## 6. Explicitly not implemented in M1

M1 does not implement:

- EP mode
- MP mode
- owner warp selection or ownership tracking
- WST/WRC
- scoreboard dependency based owner release
- compute-ready priority in MP
- non-owner L1 hit-only / miss-NACK behavior
- cache access re-execution queue
- one-memory-instruction-per-warp re-exec invariant
- scheduler issue order changes
- cache request return behavior changes

## 7. Validation performed

- Build:
  - `source setup_environment release && make -j2`
  - result: passed
- Grep checks:
  - verified new knobs, accessors, LSU helpers, aggregation, and `paper_mascar_l1_sat_*` stats exist
  - verified `-gpgpu_enable_mascar` still defaults to `0`
  - verified `SM7_QV100_mascar_l1sat_probe_on` has `-gpgpu_mascar_enable_scheduling 0`
- Smoke run:
  - skipped; the obvious short-test scripts require external workload/config environment variables (`CONFIG`, `GPUAPPS_ROOT`) and would launch benchmark infrastructure. M1 guidance allows skipping non-obvious or potentially long smoke runs.

## 8. Risks and follow-up for M2

- The M1 saturation flag is sampled at L1D access attempts, not at scheduler cycle boundaries. M2 must decide how to expose the latest or aggregate saturation state to scheduler mode selection without changing request behavior.
- `mshr_full_for_addr` is address-specific, while aggregate MSHR pressure is table-wide. M2 should define whether MP mode triggers on either condition or only aggregate saturation.
- The probe records `RESERVATION_FAIL` after the existing cache access call returns it. It does not distinguish all fail sub-reasons beyond the sampled MSHR/miss queue snapshot.
- M2 should add explicit EP/MP cycle counters and owner-warp stats rather than reusing M1 sample counters as policy counters.
- Any future non-owner miss-NACK or re-exec work must preserve the M1 read-only cache snapshot path and keep behavior changes behind separate knobs.
