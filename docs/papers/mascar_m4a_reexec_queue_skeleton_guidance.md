# Mascar M4A Detailed Guidance: Re-execution Queue Skeleton and Passive Telemetry

## Stage position

This is M4A.

M4S preflights M2/M3 active code.
M4A adds the re-execution queue data structure, ownership model, passive/would-run stats, and enqueue decision plumbing.
M4B will activate load-only re-execution behavior.

M4A should compile before M4B begins.

## Paper target summarized

The paper adds a re-execution queue alongside the LSU. When a request stalls because the L1 cannot accept a miss due to memory back pressure, the generated address and metadata are removed from the LSU pipeline and pushed into the queue. The LSU can then process another warp. Queue entries are retried later. If the queue is full, new memory instructions stop and the LSU only issues from the re-execution queue. The paper uses 32 entries and allows only one memory instruction per warp in the queue to preserve order.

M4A builds this structure but should not yet change behavior.

## New knobs

Add to shader_core_config near existing Mascar M3 knobs:

- int gpgpu_mascar_enable_reexec_queue_probe
- int gpgpu_mascar_enable_reexec_queue
- int gpgpu_mascar_reexec_loads_only
- int gpgpu_mascar_reexec_owner_takeover
- unsigned gpgpu_mascar_reexec_queue_size
- unsigned gpgpu_mascar_reexec_issue_per_cycle
- unsigned gpgpu_mascar_reexec_nack_rotate_limit

Suggested defaults:

- gpgpu_mascar_enable_reexec_queue_probe = 0
- gpgpu_mascar_enable_reexec_queue = 0
- gpgpu_mascar_reexec_loads_only = 1
- gpgpu_mascar_reexec_owner_takeover = 1
- gpgpu_mascar_reexec_queue_size = 32
- gpgpu_mascar_reexec_issue_per_cycle = 1
- gpgpu_mascar_reexec_nack_rotate_limit = 128

Meaning:

- probe enables passive would-enqueue/would-retry stats.
- active reexec queue enables M4B behavior.
- loads_only keeps M4 safe and focused.
- owner_takeover lets the head warp acquire ownership if MP mode has no owner.
- queue_size follows the paper's 32-entry design.
- issue_per_cycle limits retry bandwidth.
- nack_rotate_limit is a safety guard.

Do not make active queue default on.

## Data structure in ldst_unit

Add a small struct in shader.h near ldst_unit declarations:

  struct mascar_m4_reexec_entry {
    mem_fetch *mf;
    unsigned warp_id;
    unsigned long long enqueue_cycle;
    unsigned retry_count;
    unsigned nack_count;
    unsigned enqueue_reason;
    bool from_l1_latency_queue;
  };

Use enum or constants for enqueue_reason:
  M3_NACK
  L1_RESERVATION_FAIL
  QUEUE_FULL_RETRY
  OTHER

Add to ldst_unit:

- std::deque<mascar_m4_reexec_entry> m_mascar_m4_reexec_q
- std::vector<unsigned char> m_mascar_m4_warp_has_reexec
- unsigned long long m_mascar_m4_last_retry_cycle if needed

Initialize in ldst_unit constructor:
  queue empty
  warp_has_reexec sized max_warps_per_shader and zeroed

Ownership rule:
  The re-exec queue owns mem_fetch pointers while entries are queued.
  On hit completion, queue deletes mf.
  On miss accepted by cache, cache/memory system owns mf and queue must not delete it.
  On NACK or RESERVATION_FAIL, queue keeps mf and rotates entry to tail.
  On enqueue failure, do not steal mf ownership.

## New M4 stats

Add a plain POD stats struct or extend existing style as paper_mascar_m4_*.

Required printed stats:

- paper_mascar_m4_probe_enabled
- paper_mascar_m4_reexec_enabled
- paper_mascar_m4_queue_capacity
- paper_mascar_m4_queue_occupancy_max
- paper_mascar_m4_enqueue_attempt
- paper_mascar_m4_enqueue_success
- paper_mascar_m4_enqueue_fail_full
- paper_mascar_m4_enqueue_fail_warp_in_queue
- paper_mascar_m4_enqueue_skip_disabled
- paper_mascar_m4_enqueue_skip_nonload
- paper_mascar_m4_enqueue_m3_nack
- paper_mascar_m4_enqueue_l1_reservation_fail
- paper_mascar_m4_retry_attempt
- paper_mascar_m4_retry_hit
- paper_mascar_m4_retry_miss_sent
- paper_mascar_m4_retry_nack
- paper_mascar_m4_retry_reservation_fail
- paper_mascar_m4_retry_requeue
- paper_mascar_m4_retry_owner_takeover
- paper_mascar_m4_queue_full_stall_new
- paper_mascar_m4_warp_in_queue_block
- paper_mascar_m4_deadlock_guard

Aggregation should follow M1/M2/M3 style:
  ldst_unit owns queue counters
  shader_core_ctx forwards if needed
  simt_core_cluster aggregates
  gpgpu_sim::print_stats prints

## M4A helper methods

Add helper methods in ldst_unit. Names may vary, but keep semantics clear:

- bool mascar_m4_probe_enabled() const
- bool mascar_m4_active_enabled() const
- bool mascar_m4_is_load_candidate(mem_fetch *mf, const warp_inst_t &inst) const
- bool mascar_m4_warp_has_entry(unsigned wid) const
- bool mascar_m4_queue_full() const
- bool mascar_m4_should_enqueue(mem_fetch *mf, const warp_inst_t &inst, bool m3_nack, enum cache_request_status status) const
- bool mascar_m4_try_enqueue(mem_fetch *mf, const warp_inst_t &inst, unsigned reason, bool from_latency_queue)
- void mascar_m4_clear_warp_entry(unsigned wid)
- void mascar_m4_note_would_enqueue(mem_fetch *mf, const warp_inst_t &inst, bool m3_nack, enum cache_request_status status)

In M4A:
  helper methods can be called passively to count would-enqueue, but must not pop accessq, move latency queue entries, or change normal behavior.

Candidate enqueue conditions:
  gpgpu_enable_mascar
  reexec probe or active enabled
  L1D path
  load instruction if loads_only is true
  status == RESERVATION_FAIL, especially:
    active M3 NACK
    real L1 reservation fail
  not write
  not atomic
  queue not full
  warp does not already have reexec entry

If uncertain, skip enqueue and count a skip reason.

## Passive M4A instrumentation points

Add passive would-enqueue observation at the same points where M3 currently handles access:

1. ldst_unit::process_memory_access_queue_l1cache no-latency L1D path
   After mascar_m3_l1d_access returns status/m3_nack, before normal process_cache_access.

2. ldst_unit::L1_latency_queue_cycle
   After mascar_m3_l1d_access returns status/m3_nack, before the status branch.

In M4A:
  only call note_would_enqueue.
  do not change status handling.

## Config for M4A

Add:

  configs/hrl-repro/SM7_QV100_mascar_m4_reexec_probe_on/

Start from M3 active config.

Required settings:

  -gpgpu_enable_mascar 1
  -gpgpu_mascar_enable_l1_saturation_probe 1
  -gpgpu_mascar_enable_mp_owner_telemetry 1
  -gpgpu_mascar_enable_mp_owner_scheduling 1
  -gpgpu_mascar_enable_nonowner_hit_only_probe 1
  -gpgpu_mascar_enable_nonowner_hit_only 1
  -gpgpu_mascar_enable_reexec_queue_probe 1
  -gpgpu_mascar_enable_reexec_queue 0
  -gpgpu_mascar_reexec_queue_size 32
  -gpgpu_mascar_reexec_loads_only 1
  -gpgpu_mascar_enable_scheduling 0
  -gpgpu_mascar_enable_would_deprioritize 0

README must state:
  passive M4A only
  observes would-enqueue/would-retry opportunities
  does not change cache/LSU behavior

## M4A documentation

Create:

  docs/papers/mascar_m4a_reexec_queue_skeleton.md

Required sections:

1. Goal
2. Paper mechanism mapping
3. Queue entry representation
4. Ownership and mem_fetch lifetime
5. One-entry-per-warp policy
6. New knobs
7. New stats
8. Passive instrumentation points
9. Why M4A is behavior-safe
10. Plan for M4B active mode

## M4A validation before M4B

Run:

  git diff --check
  source setup_environment release && make -j2
  grep new M4 knobs
  grep paper_mascar_m4_ stats
  verify M4A config has active queue off

If build fails, fix before M4B.
If queue ownership cannot be represented safely, document and stop before M4B.
