# Mascar M3A Detailed Guidance: Passive Non-owner L1 Hit-only Probe

## Stage position

This is M3A of the Mascar reproduction.

M0 documented the old approximate/proxy Mascar.
M1 added passive L1 saturation probe.
M2 added EP/MP owner-warp scheduling and active scheduler-level non-owner memory blocking.
M3 adds the paper mechanism where non-owner memory requests can still access L1 in MP mode if they hit, but cannot send misses to L2.

M3A is passive only:
  It adds a cache-side hit-only probe and would-hit / would-NACK telemetry.
  It must not change scheduler behavior.
  It must not change cache access behavior.
  It must not send or suppress real memory requests.
  It must not implement a re-execution queue.

M3B will make the hit-only / miss-NACK behavior active.

## Paper target summarized for M3

The paper says that once Mascar enters MP mode, non-owner warps normally cannot issue memory requests to LSU. However, memory-ready non-owner instructions already present in the queue may be allowed to access the L1 data cache. If a non-owner request hits in L1, data returns and the instruction can commit. If it misses, L1 does not allow it to go to L2 and returns a negative acknowledgement. The paper also states that NACKs may still stall the LSU; the re-execution queue in Section III-C is the later fix for that.

M3 implements only the hit-only / miss-NACK part.
M3 does not implement the re-execution queue.

## Required M3A scope

### A. New config knobs

Add these to shader_core_config near existing Mascar knobs:

- int gpgpu_mascar_enable_nonowner_hit_only_probe
- int gpgpu_mascar_enable_nonowner_hit_only
- int gpgpu_mascar_nonowner_hit_only_loads_only
- unsigned gpgpu_mascar_nonowner_nack_release_threshold

Suggested defaults:

- gpgpu_mascar_enable_nonowner_hit_only_probe = 0
- gpgpu_mascar_enable_nonowner_hit_only = 0
- gpgpu_mascar_nonowner_hit_only_loads_only = 1
- gpgpu_mascar_nonowner_nack_release_threshold = 64

Meaning:

- nonowner_hit_only_probe enables passive M3A telemetry.
- nonowner_hit_only enables active M3B behavior.
- loads_only keeps M3 correctness manageable; stores/atomics/texture are not actively hit-only in M3.
- nack_release_threshold is a safety guard for active M3B.

Do not reuse old proxy knobs.
Do not make active M3 default on.

### B. Cache-side hit-only probe helper

Add a passive read-only helper to l1_cache or baseline_cache.

Suggested API:

- enum cache_request_status mascar_l1_hit_only_probe(new_addr_type addr, mem_fetch *mf) const

Expected semantics:

- Does not modify tag state.
- Does not update LRU.
- Does not update cache stats.
- Does not allocate MSHR.
- Does not push to miss queue.
- Does not send read/write/writeback events.
- Returns HIT only when the data is actually usable as an L1 hit.
- Treats MISS, SECTOR_MISS, HIT_RESERVED, MSHR_HIT, and RESERVATION_FAIL as not-hit for M3.

Implementation guidance:

- Inspect existing data_cache::access and tag_array::probe implementation.
- Prefer using tag_array::probe with probe_mode=true if that is read-only.
- If there is uncertainty about whether HIT_RESERVED means usable data, treat it as NACK/not-hit. M3 should be conservative.
- Do not call the normal cache access method from this passive helper.

If tag_array::probe cannot be used without side effects, stop before M3B and document the risk.

### C. Passive M3 stats

Add a new plain POD stats struct, or extend a Mascar stats struct, keeping names clear.

Suggested stat names to print as paper_mascar_m3_*:

- paper_mascar_m3_nonowner_hit_only_probe_enabled
- paper_mascar_m3_nonowner_hit_only_enabled
- paper_mascar_m3_probe_attempt
- paper_mascar_m3_probe_hit
- paper_mascar_m3_probe_nack
- paper_mascar_m3_probe_nack_miss
- paper_mascar_m3_probe_nack_reserved
- paper_mascar_m3_probe_skip_not_mp
- paper_mascar_m3_probe_skip_no_owner
- paper_mascar_m3_probe_skip_owner
- paper_mascar_m3_probe_skip_nonload
- paper_mascar_m3_probe_skip_disabled

M3A counters can live in shader_core_ctx for easy aggregation, because ldst_unit can call m_core helpers.

Suggested helper methods in shader_core_ctx:

- bool mascar_m3_probe_enabled() const
- bool mascar_m3_active_hit_only_enabled() const
- bool mascar_m3_is_nonowner_candidate(unsigned warp_id, const warp_inst_t &inst) const
- void mascar_m3_note_probe_skip_...(optional)
- void mascar_m3_note_probe_result(unsigned warp_id, enum cache_request_status status)
- void get_mascar_m3_stats(mascar_m3_stats &stats) const

### D. Passive sampling point

Sample in ldst_unit L1D access path before normal cache access.

Relevant locations:

- ldst_unit::process_memory_access_queue_l1cache()
- ldst_unit::L1_latency_queue_cycle()

M3A passive sampling should run before the normal m_L1D->access() call.

Conditions:

- gpgpu_enable_mascar is nonzero.
- gpgpu_mascar_enable_nonowner_hit_only_probe is nonzero.
- M2 owner telemetry is available.
- Current mode is MP.
- owner is valid.
- warp_id != owner.
- instruction is a load if loads_only is true.
- cache is L1D, not constant/texture/shared.

Then:

- Allocate/inspect the mem_fetch already present in this path.
- Call cache->mascar_l1_hit_only_probe(addr, mf).
- Count HIT as would-hit.
- Count all other statuses as would-NACK.
- Continue with normal cache access unchanged.

Important:
  M3A must not call process_cache_access with the passive probe result.
  M3A must not alter events.
  M3A must not pop accessq.
  M3A must not delete mf.
  M3A must not set reply.

### E. Passive config

Add:

- configs/hrl-repro/SM7_QV100_mascar_m3_hitonly_probe_on/

Start from M2 owner telemetry or M2 owner sched config, but keep M3A passive.

Required settings:

- -gpgpu_enable_mascar 1
- -gpgpu_mascar_enable_l1_saturation_probe 1
- -gpgpu_mascar_enable_mp_owner_telemetry 1
- -gpgpu_mascar_enable_mp_owner_scheduling 0
- -gpgpu_mascar_enable_nonowner_hit_only_probe 1
- -gpgpu_mascar_enable_nonowner_hit_only 0
- -gpgpu_mascar_nonowner_hit_only_loads_only 1
- -gpgpu_mascar_enable_scheduling 0
- -gpgpu_mascar_enable_would_deprioritize 0

README must state:
  This is passive M3A only.
  It observes would-hit / would-NACK opportunities.
  It does not change scheduling or cache behavior.

### F. M3A documentation

Create:

- docs/papers/mascar_m3a_nonowner_hitonly_probe.md

Required sections:

1. Goal
2. Paper mechanism mapping
3. Cache-side helper semantics
4. Passive sampling points
5. New knobs
6. New stats
7. Why HIT_RESERVED is treated conservatively
8. Why M3A is behavior-safe
9. M3B plan

### G. M3A validation before M3B

Before starting M3B, run:

- git diff --check
- source setup_environment release && make -j2
- grep for new knobs
- grep for paper_mascar_m3_probe_* stats
- verify M3A config has active hit_only off

If build fails or helper cannot be made read-only, stop before M3B.

Write intermediate notes to:

- experiments/paper-mascar/m3a_notes.md

Do not create a separate review pack unless stopping before M3B.

## Stop conditions for M3A

Stop if:

1. A passive hit-only probe cannot be implemented without mutating cache state.
2. You need to change normal cache access behavior.
3. You need to add a new cache_request_status enum just for M3A.
4. M3A starts requiring re-execution queue behavior.
5. Build fails and cannot be fixed quickly.

