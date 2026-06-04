# Mascar M3B Detailed Guidance: Active Non-owner L1 Hit-only / Miss-NACK

## Stage position

This is M3B of the Mascar reproduction.

M3A added passive non-owner L1 hit-only probe telemetry.
M3B enables the active paper-like behavior:

- In MP mode, non-owner load instructions may reach L1.
- If they hit in L1, they complete normally.
- If they miss or are reserved, they must not send a request to L2.
- Instead they get a NACK-like local retry / bank-conflict behavior.

M3B must not implement re-execution queue.
M3B must not recycle requests through a separate queue.
M3B must not implement one-memory-instruction-per-warp queue enforcement.
Those belong to M4.

## Active M3B high-level design

M2B blocks non-owner memory warps at the scheduler level.
M3B must modify this behavior so selected non-owner load instructions can issue to the LSU for L1 hit-only service.

Therefore active M3B needs two coordinated changes:

1. Scheduler side:
   Allow non-owner load instructions to issue to LSU in MP mode when M3 hit-only is enabled.
   Continue blocking non-owner stores/atomics/other memory instructions.

2. L1D/LSU side:
   For those non-owner load requests, perform hit-only cache access.
   Hit completes normally.
   Miss/HIT_RESERVED/etc. returns local NACK/BK_CONF without sending to L2.

This matches the paper stage before re-execution. NACK may still stall LSU; M4 will fix that.

## Required M3B implementation

### A. Active conditions

Active M3 behavior is enabled only when all are true:

- gpgpu_enable_mascar != 0
- gpgpu_mascar_enable_mp_owner_scheduling != 0
- gpgpu_mascar_enable_nonowner_hit_only != 0
- M2 says MP mode is active
- M2 owner is valid
- warp_id != owner_warp
- instruction is load when loads_only is true
- cache path is L1D

If any condition is false, behavior must fall back to M2B or baseline.

### B. Scheduler-side change

Update the active M2 memory gate.

Current M2B helper likely behaves as:
  if MP and owner exists and warp != owner, block non-owner memory at scheduler.

M3B must refine this:

- If active M3 hit-only is disabled:
    keep M2B behavior.

- If active M3 hit-only is enabled:
    for non-owner load instructions:
      do not block at scheduler.
      increment paper_mascar_m3_nonowner_lsu_probe_allowed.
      allow issue_warp to send the instruction to memory pipe.
    for non-owner store/atomic/other memory instructions:
      keep blocking at scheduler.
      increment paper_mascar_m3_nonowner_lsu_probe_block_nonload.

Suggested helper:

- bool shader_core_ctx::mascar_m3_allow_nonowner_lsu_probe(unsigned warp_id, const warp_inst_t *pI) const

Use it inside mascar_m2_should_block_memory_warp:

Pseudo logic:

  if not M2 active or not MP or not memory:
      return false
  if no owner:
      acquire owner
      return false
  if owner == warp_id:
      return false
  if mascar_m3_allow_nonowner_lsu_probe(warp_id, pI):
      note m3 nonowner_lsu_probe_allowed
      return false
  note m2 nonowner block
  return true

Important:
  Do not allow non-owner stores/atomics in M3 active mode unless you can prove no side-effect risk. The default must be load-only.

### C. L1D-side active hit-only access

Add active wrapper in ldst_unit for L1D access.

Suggested helper in ldst_unit:

- enum cache_request_status mascar_m3_l1d_access(l1_cache *cache, mem_fetch *mf, warp_inst_t &inst, std::list<cache_event> &events, bool &m3_nack)

Behavior:

1. Initialize m3_nack = false.
2. If active M3 conditions are false:
     return cache->access(...).
3. If active M3 conditions true:
     call cache->mascar_l1_hit_only_probe(addr, mf)
4. If probe status is HIT:
     call a new cache method that performs a load hit only, or safely call a hit-only access method.
     Must not send L2 request.
     Must allow normal hit completion and stats update.
     return HIT.
5. If probe status is not HIT:
     m3_nack = true.
     note M3 NACK stats.
     return RESERVATION_FAIL as the local retry signal, but do not call cache->access and do not send events.

Do not call normal cache->access for a non-owner active M3 miss.

### D. Cache-side active hit-only method

Passive probe alone is not enough for active HIT because the hit must complete normally and update normal hit state.

Add a safe hit-only access method to l1_cache or data_cache, for loads only:

Suggested API:

- enum cache_request_status mascar_l1_access_hit_only(new_addr_type addr, mem_fetch *mf, unsigned time, std::list<cache_event> &events)

Semantics:

- Only supports loads in M3.
- First probe tag with read-only probe.
- If probe result is not HIT:
    return RESERVATION_FAIL or a local NACK indication to caller.
- If probe result is HIT:
    perform the same cache-side operations as a normal read hit.
    It may update LRU and normal cache hit stats.
    It must not allocate MSHR.
    It must not push miss queue.
    It must not send READ_REQUEST_SENT / WRITE_REQUEST_SENT events.
    It must not call send_read_request.
    It must not send writeback.

Implementation guidance:

- Inspect data_cache::access and data_cache::process_tag_probe.
- If process_tag_probe(false, HIT, ...) is safe and does not send lower-level requests for HIT, call it after a read-only probe confirms HIT.
- Assert or guard that mf is not write.
- If status becomes unexpectedly non-HIT, return RESERVATION_FAIL and mark M3 NACK.

Do not implement hit-only for stores in M3 unless trivial and safe. The default path must be load-only.

### E. Handling L1 latency queue and no-latency paths

Both paths must use M3 active logic.

No-latency path:
  ldst_unit::process_memory_access_queue_l1cache() direct m_L1D->access path.
  Use mascar_m3_l1d_access wrapper instead of direct cache->access.

Latency queue path:
  ldst_unit::L1_latency_queue_cycle().
  Use the same wrapper for mf_next.
  If m3_nack is true, leave mf_next in l1_latency_queue[j][0] so it retries later.
  Do not delete mf_next.
  Do not call mascar_note_l1_reservation_fail for M3 NACK, because this is a Mascar NACK, not real structural reservation failure.

Existing normal RESERVATION_FAIL behavior should remain unchanged for non-M3 paths.

### F. NACK / retry semantics

For active M3 NACK:

- Return BK_CONF / RESERVATION_FAIL-style local retry behavior.
- Do not pop accessq.
- Do not delete mf in the latency-queue path.
- In no-latency process_cache_access path, returning RESERVATION_FAIL will delete the transient mf and leave accessq intact, which is acceptable.
- Do not send L2 request.
- Do not change cache miss queue.
- Count NACK stats.

This is intentionally not the final paper mechanism; M4 will move NACKed requests into re-execution queue so LSU can process other warps.

### G. NACK guard / owner release safety

Active M3 may cause repeated non-owner NACKs because there is no re-exec queue yet.

Add a simple safety guard:

- Track consecutive M3 NACKs per warp or globally.
- If the same non-owner warp gets more than gpgpu_mascar_nonowner_nack_release_threshold NACKs while owner remains valid:
    release current owner with an M3 NACK guard reason, or call a dedicated helper that clears owner.
    increment paper_mascar_m3_nack_guard_owner_release.
    reset the counter.

Rationale:
  If owner is cleared, the retried warp may proceed normally or acquire ownership later. This avoids permanent LSU blocking before M4 re-exec exists.

Do not silently allow non-owner misses to L2 just because they NACK repeatedly. Release owner instead.

### H. M3B stats required

Print these stats as paper_mascar_m3_*:

- paper_mascar_m3_nonowner_lsu_probe_allowed
- paper_mascar_m3_nonowner_lsu_probe_block_nonload
- paper_mascar_m3_hitonly_access_attempt
- paper_mascar_m3_hitonly_access_hit
- paper_mascar_m3_hitonly_access_nack
- paper_mascar_m3_hitonly_access_nack_miss
- paper_mascar_m3_hitonly_access_nack_reserved
- paper_mascar_m3_hitonly_access_nack_other
- paper_mascar_m3_hitonly_access_owner_bypass
- paper_mascar_m3_hitonly_access_mp_off_bypass
- paper_mascar_m3_nack_guard_owner_release
- paper_mascar_m3_nack_guard_threshold

If exact names differ, keep them under paper_mascar_m3_* and document them.

### I. Active config

Add:

- configs/hrl-repro/SM7_QV100_mascar_m3_hitonly_nack_on/

Start from M2B active owner scheduling config.

Required settings:

- -gpgpu_enable_mascar 1
- -gpgpu_mascar_enable_l1_saturation_probe 1
- -gpgpu_mascar_enable_mp_owner_telemetry 1
- -gpgpu_mascar_enable_mp_owner_scheduling 1
- -gpgpu_mascar_enable_nonowner_hit_only_probe 1
- -gpgpu_mascar_enable_nonowner_hit_only 1
- -gpgpu_mascar_nonowner_hit_only_loads_only 1
- -gpgpu_mascar_nonowner_nack_release_threshold 64
- -gpgpu_mascar_enable_scheduling 0
- -gpgpu_mascar_enable_would_deprioritize 0

README must state:
  This is active M3 hit-only / NACK.
  It allows non-owner loads to reach L1 in MP.
  It does not implement re-execution queue.
  Stores/atomics/non-load memory ops remain M2-blocked by default.

### J. M3B documentation

Create:

- docs/papers/mascar_m3b_active_hitonly_nack.md

Required sections:

1. Goal
2. Paper mechanism implemented
3. Scheduler-side change
4. Cache-side hit-only access
5. NACK semantics
6. L1 latency queue handling
7. NACK guard
8. New knobs
9. New stats
10. Configs
11. Validation
12. Remaining gap for M4 re-execution queue

Also update or create:

- docs/papers/mascar_m3_status.md

Summarize:
  M1 = L1 saturation
  M2 = MP owner scheduling
  M3 = non-owner hit-only / miss-NACK
  M4 remaining = re-execution queue

### K. Validation

Required checks:

1. git diff --check
2. source setup_environment release && make -j2
3. grep new knobs:
   gpgpu_mascar_enable_nonowner_hit_only_probe
   gpgpu_mascar_enable_nonowner_hit_only
   gpgpu_mascar_nonowner_hit_only_loads_only
   gpgpu_mascar_nonowner_nack_release_threshold
4. grep stats:
   paper_mascar_m3_probe_attempt
   paper_mascar_m3_nonowner_lsu_probe_allowed
   paper_mascar_m3_hitonly_access_attempt
   paper_mascar_m3_hitonly_access_nack
5. config check:
   M3A probe config active hit-only off.
   M3B active config active hit-only on.
   old proxy scheduling off in both.
6. Optional smoke:
   If an obvious short smoke runner exists, run one tiny workload with M3B config.
   If benchmark env is missing or would be long, skip and document.

Do not run full benchmark suites.

### L. Postcheck and review pack

Create:

- experiments/paper-mascar/m3_postcheck.md
- experiments/paper-mascar/m3_diff_name_status.txt
- experiments/paper-mascar/m3_symbol_grep.txt

Postcheck must include:

- start_iso
- end_iso
- elapsed_sec using start_ts/end_ts
- current branch and HEAD
- git status before and after
- changed files
- M3A summary
- M3B summary
- build result
- smoke result or reason skipped
- grep/config checks
- review pack path
- warnings and limitations

Review pack:

- /workspace/tmp/mascar_m3_review_pack_YYYYMMDD_HHMMSS.tar.gz

Include all changed source/config/docs/postcheck/helper files.

Do not commit M3 implementation outputs.

## Stop conditions for M3B

Stop and report if:

1. hit-only active access cannot be implemented without possibly sending L2 requests on miss.
2. normal cache access return behavior must be changed globally.
3. new enum values are required across broad cache code.
4. M3 requires a re-execution queue to work at all.
5. build fails and cannot be fixed quickly.
6. active M3 causes obvious deadlock in a short smoke and NACK guard cannot recover.
7. elapsed time exceeds 110 minutes.

## Final report to GPT

Report only:

1. elapsed_sec
2. review pack path
3. git status --short
4. build/smoke status
5. files GPT should review

