# Mascar M4B Detailed Guidance: Active Load-only Cache Access Re-execution Queue

## Stage position

This is M4B.

M4A created the queue skeleton and passive stats.
M4B activates load-only re-execution.

M4B is allowed to modify LSU/cache request lifecycle, but only when:
  gpgpu_enable_mascar = 1
  gpgpu_mascar_enable_reexec_queue = 1

M4B must not implement stores, atomics, texture, or constant memory re-execution unless trivially safe. Default is load-only.

## Active high-level design

When a load request cannot proceed because of active M3 NACK or real L1D RESERVATION_FAIL, move it into the re-execution queue.

Then:
  free the LSU / latency queue slot
  allow other warps to make progress
  retry queued request later

On retry:
  if non-owner in MP mode, use M3 hit-only behavior:
    HIT completes
    miss/reserved gives NACK and requeues
  if owner or EP/no-owner, use normal L1D access:
    HIT completes
    MISS/HIT_RESERVED accepted to lower memory, remove from queue
    RESERVATION_FAIL requeues

## Active conditions

Active M4 behavior requires:

  gpgpu_enable_mascar != 0
  gpgpu_mascar_enable_reexec_queue != 0
  L1D path
  load instruction if loads_only is true
  mf is not write
  mf is not atomic

For initial enqueue:
  status == RESERVATION_FAIL and either:
    m3_nack is true
    real L1 reservation fail occurred

For retry:
  issue at most gpgpu_mascar_reexec_issue_per_cycle entries per cycle.

## Where to process re-exec queue

Add method:

  void ldst_unit::mascar_m4_reexec_cycle()

Call it from ldst_unit::cycle after L1D cycle / L1_latency_queue_cycle and before processing m_dispatch_reg. This lets queued requests make progress before new dispatch memory work.

Also when the queue is full:
  process_memory_access_queue_l1cache should stall new L1D loads and count queue_full_stall_new.
  The queue should still be retried each cycle by mascar_m4_reexec_cycle.

Do not process more than issue_per_cycle entries per cycle.

## Enqueue behavior: no-latency L1D path

In ldst_unit::process_memory_access_queue_l1cache no-latency path:

Current flow:
  allocate mf from inst.accessq_back
  sample M1
  probe M3
  do M3/normal cache access
  call process_cache_access

Modify:

After status/m3_nack is known, before process_cache_access:

If active M4 and status == RESERVATION_FAIL and candidate load:
  if try_enqueue succeeds:
    inst.accessq_pop_back()
    queue owns mf
    do not call process_cache_access
    return:
      COAL_STALL if inst.accessq not empty
      NO_RC_FAIL if accessq empty
  else:
    keep existing fallback behavior
    delete mf only through existing process_cache_access path
    do not pop accessq manually

This is the simulator equivalent of removing the request from the LSU pipeline and pushing it to re-exec queue.

If try_enqueue fails because queue full:
  count enqueue_fail_full
  count queue_full_stall_new
  fall back to existing RESERVATION_FAIL handling or return BK_CONF after deleting mf.
  Do not pop accessq.

If try_enqueue fails because same warp already has an entry:
  count enqueue_fail_warp_in_queue
  fall back to existing retry behavior.
  Do not pop accessq.

## Enqueue behavior: L1 latency queue path

In ldst_unit::L1_latency_queue_cycle:

After status/m3_nack is known:

If active M4 and status == RESERVATION_FAIL and candidate load:
  if try_enqueue succeeds:
    l1_latency_queue[j][0] = NULL
    queue owns mf_next
    do not delete mf_next
    continue normal latency queue shift
  else:
    leave mf_next in l1_latency_queue[j][0]
    keep existing retry/stall behavior

Important:
  accessq was already popped when the mf entered l1_latency_queue, so do not touch accessq here.

## Retry behavior

In mascar_m4_reexec_cycle:

For each issue slot up to issue_per_cycle:
  if queue empty, return
  take front entry
  mf = entry.mf
  inst = mf->get_inst()
  wid = inst.warp_id()

Before retry:
  if owner_takeover enabled and M2 MP mode active and no owner is valid:
    acquire owner for wid through a shader_core_ctx helper
    count retry_owner_takeover

Determine retry mode:

Case 1: active M3 says this is non-owner in MP:
  Use M3 active hit-only path.
  Call mascar_m3_l1d_access or equivalent so non-owner misses do not send L2.
  If HIT:
    complete reexec hit
    delete mf
    clear warp_has_reexec[wid]
    pop entry
  If RESERVATION_FAIL due M3 NACK:
    rotate entry to tail
    count retry_nack and retry_requeue
    keep warp_has_reexec[wid] set
  If unexpected status:
    treat conservatively as requeue/NACK unless it is MISS/HIT_RESERVED from owner/normal path.

Case 2: owner, no owner, or EP mode:
  Use normal m_L1D->access.
  If HIT:
    complete reexec hit
    delete mf
    clear warp_has_reexec[wid]
    pop entry
  If MISS or HIT_RESERVED:
    request accepted by cache/memory
    clear warp_has_reexec[wid]
    pop entry
    do not delete mf
  If RESERVATION_FAIL:
    note real L1 reservation fail
    rotate entry to tail
    count retry_reservation_fail and retry_requeue
    keep warp_has_reexec[wid] set

Never send a non-owner MP miss to L2.
Never delete mf after MISS/HIT_RESERVED accepted.
Never leave warp_has_reexec set after entry leaves queue permanently.

## Completing a re-exec hit

Add helper:

  void ldst_unit::mascar_m4_complete_reexec_hit(mem_fetch *mf)

Because the original instruction may have left the LSU pipeline, re-exec HIT must complete scoreboard/register bookkeeping similarly to the L1 latency queue HIT path.

For load only:
  for each output register:
    decrement m_pending_writes[wid][reg] if present
    if count reaches zero:
      erase reg entry
      m_scoreboard->releaseRegister(wid, reg)
      m_core->warp_inst_complete(inst)

Also handle LDGSTS dependency if present, mirroring existing latency-queue HIT path.

Do not support store hit completion in M4B unless trivial and safe. The active candidate should be load-only by default.

Be careful not to double-release:
  Only use this helper for queued entries whose original accessq was popped and whose request is owned by the re-exec queue.
  Do not use it for normal cache HIT in process_cache_access.

## One memory instruction per warp

Use m_mascar_m4_warp_has_reexec[wid].

On successful enqueue:
  if bit already set:
    fail enqueue and do not pop original request
  else:
    set bit

On queue entry leaves permanently:
  HIT completed
  MISS/HIT_RESERVED accepted
  explicit cleanup
clear bit.

On requeue tail:
  keep bit set.

This is conservative. It may serialize multiple coalesced requests from the same warp more than the paper, but preserves order and avoids duplicate ownership.

## Queue full behavior

If queue is full:
  new enqueue attempts fail
  new L1D loads should not steal additional queue entries
  count queue_full_stall_new
  reexec_cycle keeps retrying existing entries

This matches the paper statement that when the re-execution queue is full, the LSU is forced to stall and only issues from the queue.

## Active config

Add:

  configs/hrl-repro/SM7_QV100_mascar_m4_reexec_load_on/

Start from M3 active hitonly_nack config.

Required settings:

  -gpgpu_enable_mascar 1
  -gpgpu_mascar_enable_l1_saturation_probe 1
  -gpgpu_mascar_enable_mp_owner_telemetry 1
  -gpgpu_mascar_enable_mp_owner_scheduling 1
  -gpgpu_mascar_enable_nonowner_hit_only_probe 1
  -gpgpu_mascar_enable_nonowner_hit_only 1
  -gpgpu_mascar_enable_reexec_queue_probe 1
  -gpgpu_mascar_enable_reexec_queue 1
  -gpgpu_mascar_reexec_loads_only 1
  -gpgpu_mascar_reexec_owner_takeover 1
  -gpgpu_mascar_reexec_queue_size 32
  -gpgpu_mascar_reexec_issue_per_cycle 1
  -gpgpu_mascar_enable_scheduling 0
  -gpgpu_mascar_enable_would_deprioritize 0

README must state:
  active M4 load-only re-exec
  non-owner MP miss still NACKs and requeues
  owner/EP miss may send to lower memory
  no store/atomic/texture re-exec yet

## Documentation

Create:

  docs/papers/mascar_m4b_active_load_reexec.md

Required sections:

1. Goal
2. Paper mechanism implemented
3. Queue lifecycle
4. Enqueue conditions
5. Retry behavior
6. Non-owner NACK and owner miss behavior
7. Hit completion and scoreboard handling
8. Queue full behavior
9. One-entry-per-warp rule
10. New knobs
11. New stats
12. Known limitations
13. Remaining gaps after M4

## Validation

After M4B:

  git diff --check
  source setup_environment release && make -j2

Grep:
  gpgpu_mascar_enable_reexec_queue
  gpgpu_mascar_reexec_queue_size
  paper_mascar_m4_enqueue_success
  paper_mascar_m4_retry_hit
  paper_mascar_m4_retry_requeue

Config check:
  M4A config active queue off
  M4B config active queue on
  old proxy scheduling off

If smoke runner exists, run one small M4B active smoke with timeout 20m.
If smoke fails, debug within M4B/M4C.

## Stop conditions

Stop only if:
  implementing re-exec requires unsafe global cache status changes
  mem_fetch ownership cannot be kept safe
  build fails and cannot be fixed
  same code path needs broad store/atomic support to compile
  elapsed time exceeds 120 minutes

Do not stop just because active smoke fails. Debug it.
