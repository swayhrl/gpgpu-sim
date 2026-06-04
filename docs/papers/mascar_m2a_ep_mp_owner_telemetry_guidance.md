# Mascar M2A Detailed Guidance: Passive EP/MP and Owner-Warp Control Plane Telemetry

## Stage position

This is M2A of the Mascar reproduction.

M0 established that the old branch was approximate/proxy Mascar.
M1 added a passive L1D saturation probe based on L1 MSHR and miss queue pressure.
M2A must build the paper-like EP/MP and owner-warp control plane in passive telemetry mode.
M2B will use the M2A control plane to enable active owner-warp scheduling.

M2A must not change scheduler behavior.
M2A must not block or reorder warps.
M2A must not implement re-execution.
M2A must not implement non-owner hit-only or miss-NACK.
M2A must only add knobs, state, passive tracking, stats, docs, and passive configs.

## Paper target summarized for M2A

Mascar has two scheduler modes.

EP mode:
  Used when memory is not saturated.
  The scheduler should behave like the normal scheduler, and memory-ready warps can be prioritized to keep the memory pipeline full.

MP mode:
  Used when memory saturation is detected.
  One owner warp is selected.
  That owner warp has priority to issue memory requests.
  Non-owner warps should not issue memory instructions in active M2B.
  Compute-ready warps should be prioritized over memory-ready warps in MP to improve memory/compute overlap.

WST and WRC:
  WST has two conceptual bits per warp:
    memory-op bit: next instruction is a memory instruction.
    stall bit: warp should be stalled because it is a non-owner memory warp or scoreboard-blocked.
  WRC determines these bits by inspecting the I-buffer, scoreboard, owner state, and saturation flag.

Owner management:
  Owner continues issuing memory instructions while saturated.
  Owner relinquishes when it reaches an instruction dependent on its own long-latency load.
  In GPGPU-Sim, a reasonable paper-like approximation is to release owner when the owner warp reaches a scoreboard collision on its next instruction, because that usually means it waits on pending writes from earlier memory operations.
  Also release owner when saturation clears, warp exits, or a deadlock/no-progress guard triggers.

Multiple schedulers:
  Owner state must live in shader_core_ctx, not in scheduler_unit, so all schedulers in the same SM see the same owner.

## Required M2A scope

### A. New config knobs

Add these knobs to shader_core_config near existing Mascar knobs:

- int gpgpu_mascar_enable_mp_owner_telemetry
- int gpgpu_mascar_enable_mp_owner_scheduling
- int gpgpu_mascar_m2_compute_first
- unsigned gpgpu_mascar_l1_saturation_recent_window
- unsigned gpgpu_mascar_owner_max_hold_cycles
- unsigned gpgpu_mascar_owner_no_progress_limit

Suggested defaults:

- gpgpu_mascar_enable_mp_owner_telemetry = 0
- gpgpu_mascar_enable_mp_owner_scheduling = 0
- gpgpu_mascar_m2_compute_first = 1
- gpgpu_mascar_l1_saturation_recent_window = 8
- gpgpu_mascar_owner_max_hold_cycles = 256
- gpgpu_mascar_owner_no_progress_limit = 64

Meaning:

- mp_owner_telemetry enables M2A passive EP/MP/owner stats.
- mp_owner_scheduling enables M2B active scheduling. It must default off.
- compute_first controls the M2B MP-mode compute-ready priority behavior.
- l1_saturation_recent_window converts M1 sample events into a scheduler-visible recent saturation flag.
- owner_max_hold_cycles and owner_no_progress_limit are deadlock guards.

Important:
  Do not reuse gpgpu_mascar_enable_scheduling for M2. That knob belongs to the old approximate/proxy skip policy.
  M2 active scheduling must use gpgpu_mascar_enable_mp_owner_scheduling.

### B. M1 saturation flag extension

M1 currently counts saturation samples in ldst_unit.
M2A must expose a recent boolean saturation flag to shader_core_ctx.

In ldst_unit, add passive state:

- bool m_mascar_l1_sat_recent_flag
- unsigned long long m_mascar_l1_sat_last_update_cycle
- unsigned long long m_mascar_l1_sat_recent_set
- unsigned long long m_mascar_l1_sat_recent_clear

In mascar_sample_l1_saturation:

- compute saturated as M1 already does:
    mshr_full_for_addr OR mshr_entries_almost_full OR miss_queue_full OR miss_queue_almost_full
- set m_mascar_l1_sat_recent_flag = saturated
- set m_mascar_l1_sat_last_update_cycle = current gpu cycle
- increment recent_set when saturated is true
- increment recent_clear when saturated is false

In mascar_note_l1_reservation_fail:

- set m_mascar_l1_sat_recent_flag = true
- set m_mascar_l1_sat_last_update_cycle = current gpu cycle
- increment recent_set or a separate reservation-based set counter if useful

Add const helper in ldst_unit:

- bool mascar_l1_saturation_recent(unsigned recent_window) const

Expected behavior:

- returns false if probe disabled or no recent sample.
- returns false if recent flag is false.
- returns true only if current cycle - last_update_cycle <= recent_window.
- guard against underflow.

Add forwarding helper:

- shader_core_ctx::mascar_l1_saturation_flag() const

This is the paper-like memory saturation flag for M2. It is not perfect because it comes from recent sampled L1D accesses, but it is much closer to the paper than the old m_mem_out stall-streak proxy.

### C. M2A state in shader_core_ctx

Add state to shader_core_ctx so it is shared by all scheduler_unit objects in the same SM:

- bool m_mascar_m2_mp_mode
- bool m_mascar_m2_owner_valid
- unsigned m_mascar_m2_owner_warp
- unsigned long long m_mascar_m2_last_cycle_update
- unsigned long long m_mascar_m2_owner_acquire_cycle
- unsigned long long m_mascar_m2_owner_last_mem_issue_cycle
- std::vector<unsigned char> m_mascar_m2_wst_mem_bit
- std::vector<unsigned char> m_mascar_m2_wst_stall_bit

Initialize in shader_core_ctx constructor:
  mp_mode false
  owner_valid false
  owner_warp 0
  last cycle update invalid
  owner cycle fields 0
  WST vectors sized max_warps_per_shader and filled with 0

Reset WST state when appropriate in reinit if needed.

### D. M2A helper methods in shader_core_ctx

Add helpers. Names can vary, but keep them clear.

Suggested helpers:

- bool mascar_m2_telemetry_enabled() const
- bool mascar_m2_active_scheduling_enabled() const
- bool mascar_inst_is_memory(const warp_inst_t *pI) const
- bool mascar_inst_is_compute(const warp_inst_t *pI) const
- void mascar_m2_cycle_begin()
- void mascar_m2_note_candidate(unsigned warp_id, const warp_inst_t *pI, bool scoreboard_blocked)
- void mascar_m2_note_memory_issue_attempt(unsigned warp_id, const warp_inst_t *pI)
- void mascar_m2_note_memory_issue_success(unsigned warp_id)
- void mascar_m2_note_compute_issue_success(unsigned warp_id)
- void mascar_m2_note_scoreboard_block(unsigned warp_id, const warp_inst_t *pI)
- bool mascar_m2_owner_valid() const
- unsigned mascar_m2_owner_warp() const
- bool mascar_m2_mp_mode() const

Memory instruction classification should match scheduler_unit::cycle existing memory path:
  LOAD_OP
  STORE_OP
  MEMORY_BARRIER_OP
  TENSOR_CORE_LOAD_OP
  TENSOR_CORE_STORE_OP

Compute instruction classification for M2 can be:
  not memory, not invalid, not waiting, not ibuffer empty.
  Treat SP/INT/DP/SFU/TENSOR_CORE/SPEC units as compute-like for priority/stats.
  Do not treat memory barrier as compute.

mascar_m2_cycle_begin should:
  do nothing unless gpgpu_enable_mascar and mp_owner_telemetry are enabled.
  update at most once per SM per gpu cycle using last_cycle_update guard.
  read current saturation via mascar_l1_saturation_flag().
  if saturation true:
    mp_mode true
    increment mp_cycles
  else:
    mp_mode false
    increment ep_cycles
    if owner_valid, release owner with saturation_clear reason
  if owner_valid, increment owner_valid_cycles
  if owner hold exceeds owner_max_hold_cycles, release with max_hold reason
  if owner has no memory issue progress longer than owner_no_progress_limit, release with no_progress reason
  clear or refresh WST bits for current cycle if appropriate.

Important:
  Because multiple schedulers can call scheduler_unit::cycle in the same SM cycle, cycle_begin must not double count cycles.

### E. Passive owner acquisition in M2A

M2A must not change behavior, but it may maintain a would-be owner state.

When MP mode is active and a ready memory instruction is observed:

- if no owner is valid:
    acquire owner = warp_id
    increment owner_acquire
- if owner is valid and warp_id == owner:
    increment would_owner_mem_issue or owner_mem_ready
- if owner is valid and warp_id != owner:
    increment would_block_nonowner_mem
    set WST stall bit for this warp
    do not block in M2A

When a ready compute instruction is observed during MP:
  increment compute_ready_in_mp
  if compute_first enabled, increment would_prioritize_compute_in_mp

When the owner warp encounters scoreboard collision:
  release owner with scoreboard reason.
  This is the simulator approximation for the paper's owner relinquish on dependency on issued long-latency loads.

When owner warp exits or is done:
  release owner with warp_done reason.

### F. Stats required from M2A

Add counters and print them under paper_mascar_* stats in gpu-sim.cc.

Required stat names:

- paper_mascar_m2_owner_telemetry_enabled
- paper_mascar_m2_owner_scheduling_enabled
- paper_mascar_m2_ep_cycles
- paper_mascar_m2_mp_cycles
- paper_mascar_m2_owner_valid_cycles
- paper_mascar_m2_owner_acquire
- paper_mascar_m2_owner_release
- paper_mascar_m2_owner_release_saturation_clear
- paper_mascar_m2_owner_release_scoreboard
- paper_mascar_m2_owner_release_warp_done
- paper_mascar_m2_owner_release_max_hold
- paper_mascar_m2_owner_release_no_progress
- paper_mascar_m2_compute_ready_in_mp
- paper_mascar_m2_memory_ready_in_mp
- paper_mascar_m2_would_block_nonowner_mem
- paper_mascar_m2_would_owner_mem_issue
- paper_mascar_m2_would_prioritize_compute
- paper_mascar_m2_wst_mem_bit_set
- paper_mascar_m2_wst_stall_bit_set
- paper_mascar_m2_l1_recent_set
- paper_mascar_m2_l1_recent_clear

Aggregation path should follow the existing style:
  ldst_unit if ldst-owned counter
  shader_core_ctx for M2 owner counters
  simt_core_cluster forwarding
  gpgpu_sim::print_stats printing

Avoid huge function signatures if possible, but keep code simple and compile-safe. A struct is acceptable if the repository style permits it. If using a struct, define it in shader.h with plain POD fields initialized clearly.

### G. Passive config

Add config directory:

- configs/hrl-repro/SM7_QV100_mascar_m2_owner_telemetry_on/

Copy from M1 passive config.

Required settings:

- -gpgpu_enable_mascar 1
- -gpgpu_mascar_enable_l1_saturation_probe 1
- -gpgpu_mascar_enable_mp_owner_telemetry 1
- -gpgpu_mascar_enable_mp_owner_scheduling 0
- -gpgpu_mascar_enable_scheduling 0
- -gpgpu_mascar_enable_would_deprioritize 0
- -gpgpu_mascar_m2_compute_first 1
- -gpgpu_mascar_l1_saturation_recent_window 8
- -gpgpu_mascar_owner_max_hold_cycles 256
- -gpgpu_mascar_owner_no_progress_limit 64

Add README explaining:
  This config is passive M2A only.
  It does not change scheduling.
  It should show EP/MP and would-owner stats if workloads create L1D saturation.

### H. M2A documentation

Create:

- docs/papers/mascar_m2a_ep_mp_owner_telemetry.md

Required sections:

1. Goal
2. Paper mechanism mapping
3. M1 signal used as M2 saturation flag
4. WST/WRC simulator approximation
5. Owner acquisition and release approximation
6. New knobs
7. New stats
8. Behavior safety: why M2A is passive
9. Known limitations
10. How M2B will turn this into active scheduling

### I. M2A validation before moving to M2B

Before starting M2B, run:

- git diff --check
- source setup_environment release && make -j2

If build fails, fix before M2B.
If M2A requires invasive behavior changes, stop and report.

Write intermediate notes into:

- experiments/paper-mascar/m2a_notes.md

Do not create a separate M2A review pack unless you stop before M2B.

## Stop conditions for M2A

Stop before M2B if:

1. M1 saturation flag cannot be exposed without changing cache behavior.
2. passive M2A changes scheduler issue order.
3. build fails and cannot be fixed quickly.
4. owner state must be kept per scheduler instead of per SM.
5. implementing M2A requires re-execution or hit-only cache changes.

