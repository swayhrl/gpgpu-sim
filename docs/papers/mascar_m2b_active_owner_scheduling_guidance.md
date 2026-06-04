# Mascar M2B Detailed Guidance: Active EP/MP Owner-Warp Scheduling

## Stage position

This is M2B of the Mascar reproduction.

M2A builds passive EP/MP and owner-warp telemetry.
M2B turns that control plane into active scheduling behavior.

M2B is allowed to change scheduler behavior, but only when:
  gpgpu_enable_mascar = 1
  gpgpu_mascar_enable_mp_owner_scheduling = 1

M2B must not implement:
  re-execution queue
  non-owner hit-only
  non-owner miss-NACK
  cache access retry
  changes to cache request acceptance behavior

Those belong to M3 and M4.

## Paper target for M2B

In MP mode, Mascar gives one owner warp priority/exclusivity for memory requests. This prevents all warps from sending only a fraction of their required requests, and instead allows one warp to obtain data and begin compute while another warp accesses memory.

In MP mode, compute-ready warps should be prioritized over memory-ready warps to maximize compute/memory overlap.

When memory saturation is relieved, Mascar returns to EP mode and clears stall state.

## M2B active behavior

### A. Active MP condition

Active MP owner scheduling is enabled only when:

- gpgpu_enable_mascar is nonzero
- gpgpu_mascar_enable_mp_owner_scheduling is nonzero
- gpgpu_mascar_enable_mp_owner_telemetry is also enabled or implicitly treated as enabled
- M1 L1 saturation recent flag says saturated

If saturation flag is false:
  EP mode.
  Do not block memory warps.
  Release owner if valid.
  Preserve baseline scheduler behavior as much as possible.

### B. Owner state must be per SM

Owner state lives in shader_core_ctx, not scheduler_unit.

Rationale:
  The paper explicitly accounts for multiple schedulers per SM by sharing owner warp information across schedulers.

If multiple scheduler units call into the same shader_core_ctx:
  They must see the same owner_valid and owner_warp.

### C. Active memory gating

Insert active gating in scheduler_unit::cycle in the memory instruction path, after:
  instruction is valid
  scoreboard has passed
  active mask has been computed
  before m_mem_out->has_free and issue_warp

Existing old proxy gate:
  mascar_skip_gate_warp(warp_id)

Keep old proxy behavior intact for old configs.
But M2 active owner scheduling must use a new call, not mascar_skip_gate_warp.

Preferred new helper:

- bool shader_core_ctx::mascar_m2_should_block_memory_warp(unsigned warp_id, const warp_inst_t *pI)

Semantics:

- If M2 active scheduling disabled: return false.
- If not in MP mode: return false.
- If instruction is not memory: return false.
- If no owner valid:
    acquire owner = warp_id
    return false
- If owner valid and owner == warp_id:
    return false
- If owner valid and owner != warp_id:
    increment nonowner_mem_block
    set WST stall bit
    return true

When blocked:
  checked++
  break current warp while-loop
  continue outer scheduler search to allow other warps, especially compute-ready warps, to issue.
  Do not call issue_warp.
  Do not call m_mem_out->has_free.
  Do not change mem_fetch/cache behavior.

This gives non-owner memory warps a scheduler-level stall in M2B. Hit-only non-owner behavior will be implemented later.

### D. Compute-ready priority in MP

The paper says MP mode prioritizes compute-ready warps over memory-ready warps.

Preferred implementation:
  After order_warps(), if active MP and compute_first enabled, apply a Mascar M2 reorder helper before iterating.

Suggested helper in scheduler_unit:

- void scheduler_unit::apply_mascar_m2_priority_ordering()

Called in scheduler_unit::cycle:

  order_warps();
  m_shader->mascar_m2_cycle_begin();
  apply_mascar_m2_priority_ordering();

Reorder only when:
  gpgpu_enable_mascar
  gpgpu_mascar_enable_mp_owner_scheduling
  gpgpu_mascar_m2_compute_first
  m_shader->mascar_m2_mp_mode()

Classification for each warp in current m_next_cycle_prioritized_warps:

1. skip null, done_exit, waiting, ibuffer_empty into tail/unready group.
2. get pI = warp(wid).ibuffer_next_inst().
3. if pI is null or invalid, keep in tail.
4. if scoreboard collision, keep in tail or scoreboard-blocked group.
5. if pI is compute-like ready:
     put in compute_ready group.
6. if pI is memory-like ready and owner valid and wid == owner:
     put in owner_memory_ready group.
7. if pI is memory-like ready and no owner valid:
     put in first_memory_ready group, preserving original order.
8. other non-owner memory-ready:
     put in nonowner_memory_ready group.
9. final order:
     compute_ready
     owner_memory_ready
     first_memory_ready
     nonowner_memory_ready
     tail

This is a scheduler ordering approximation of the paper's separate Comp_Q and Mem_Q.

Important:
  The helper may call m_scoreboard->checkCollision only if that function is read-only. It appears to be used as a predicate in scheduler_unit::cycle, so using it for ordering should be acceptable.
  Do not call issue_warp in the reorder helper.
  Do not update scoreboard, ibuffer, active masks, or SIMT stack in the reorder helper.
  If classification is uncertain, keep original order for that warp rather than forcing it.

Fallback acceptable only if needed to preserve build:
  If reorder helper causes compile or behavior risk, implement non-owner memory blocking first and document compute-first as limited to "blocked non-owner memory lets later compute warps issue". But prefer the reorder helper.

### E. Owner acquire/release active rules

Acquire owner:

- In MP mode, when a ready memory instruction reaches the active gating point and no owner is valid.
- Set owner_valid true and owner_warp = warp_id.
- Increment owner_acquire.
- Initialize owner_acquire_cycle and owner_last_mem_issue_cycle.

Owner can issue memory:

- If owner valid and warp_id == owner, allow memory instruction to proceed to m_mem_out if m_mem_out has space.
- On successful issue, increment owner_mem_issue and update owner_last_mem_issue_cycle.

Owner can execute compute:

- If owner has compute-ready instruction, allow it like any other compute.
- Increment owner_compute_issue if useful.
- Do not necessarily release owner on independent compute.

Release owner:

1. Saturation clears:
   Release immediately.
   Clear WST stall bits.

2. Owner reaches scoreboard collision:
   In scheduler_unit::cycle, in the scoreboard collision path, call:
     m_shader->mascar_m2_note_scoreboard_block(warp_id, pI)
   If warp_id is current owner and MP active, release with scoreboard reason.
   This approximates "owner reached an instruction dependent on one of its issued loads".

3. Owner done/exit:
   Release with warp_done.

4. Owner max hold:
   If current cycle - owner_acquire_cycle > owner_max_hold_cycles, release with max_hold.

5. Owner no progress:
   If current cycle - owner_last_mem_issue_cycle > owner_no_progress_limit, release with no_progress.
   This is a deadlock guard.

6. Optional:
   If owner warp has no valid instruction for a long time, release with no_progress.

Do not release owner simply because a non-owner warp is ready.
Do not release owner solely because owner issues one memory request.
The paper intent is that owner continues until it reaches a dependent instruction or saturation clears.

### F. Interaction with old proxy Mascar

Do not delete old proxy skip logic in M2.

Existing old knobs:
  gpgpu_mascar_enable_telemetry
  gpgpu_mascar_enable_would_deprioritize
  gpgpu_mascar_enable_scheduling

M2 configs must set:
  gpgpu_mascar_enable_scheduling = 0
  gpgpu_mascar_enable_would_deprioritize = 0 unless intentionally comparing proxy
  gpgpu_mascar_enable_mp_owner_scheduling = 1 for M2 active

If both old proxy scheduling and M2 owner scheduling are enabled accidentally:
  Prefer to make docs/configs say this is unsupported.
  Optionally print both stats, but do not attempt complex interaction.

### G. Active stats required

In addition to M2A stats, add active counters and print them:

- paper_mascar_m2_nonowner_mem_block
- paper_mascar_m2_owner_mem_issue
- paper_mascar_m2_owner_compute_issue
- paper_mascar_m2_owner_first_acquire
- paper_mascar_m2_compute_priority_reorder
- paper_mascar_m2_compute_priority_candidates
- paper_mascar_m2_memory_priority_candidates
- paper_mascar_m2_deadlock_force_release
- paper_mascar_m2_active_block_guard_allow

If names differ, keep them clearly under paper_mascar_m2_* and document them.

### H. Active config

Add config directory:

- configs/hrl-repro/SM7_QV100_mascar_m2_owner_sched_on/

Copy from M2A telemetry config.

Required settings:

- -gpgpu_enable_mascar 1
- -gpgpu_mascar_enable_l1_saturation_probe 1
- -gpgpu_mascar_enable_mp_owner_telemetry 1
- -gpgpu_mascar_enable_mp_owner_scheduling 1
- -gpgpu_mascar_enable_scheduling 0
- -gpgpu_mascar_enable_would_deprioritize 0
- -gpgpu_mascar_m2_compute_first 1
- -gpgpu_mascar_l1_saturation_recent_window 8
- -gpgpu_mascar_owner_max_hold_cycles 256
- -gpgpu_mascar_owner_no_progress_limit 64

README must explain:
  This is active M2 owner scheduling.
  It implements scheduler-level MP owner gating.
  It does not implement non-owner L1 hit-only, miss-NACK, or re-execution.

### I. M2B documentation

Create:

- docs/papers/mascar_m2b_active_owner_scheduling.md

Required sections:

1. Goal
2. What paper mechanism is implemented
3. Where M2 intentionally differs from the paper
4. New active scheduling knobs
5. Active owner rules
6. Compute-ready priority implementation
7. Deadlock guards
8. Stats
9. Configs
10. Validation
11. Remaining work for M3/M4

### J. Validation

Required checks:

1. git diff --check
2. source setup_environment release && make -j2
3. grep option registration:
   gpgpu_mascar_enable_mp_owner_telemetry
   gpgpu_mascar_enable_mp_owner_scheduling
   gpgpu_mascar_m2_compute_first
4. grep printed stats:
   paper_mascar_m2_ep_cycles
   paper_mascar_m2_mp_cycles
   paper_mascar_m2_owner_acquire
   paper_mascar_m2_nonowner_mem_block
5. config check:
   M2A telemetry config has scheduling off.
   M2B active config has owner scheduling on and old proxy scheduling off.
6. Optional smoke:
   If an obvious short smoke runner exists, run one tiny workload with M2B config.
   If benchmark env is missing or would be long, skip and document.

Do not run full benchmark suites.

### K. Postcheck

Create:

- experiments/paper-mascar/m2_postcheck.md

Must include:

- start_iso
- end_iso
- elapsed_sec computed using start_ts and end_ts
- current branch and HEAD
- git status before and after
- list of changed files
- M2A summary
- M2B summary
- build commands and results
- smoke commands and results or reason skipped
- grep/config checks
- review pack path
- warnings and limitations

Also create:

- experiments/paper-mascar/m2_diff_name_status.txt
- experiments/paper-mascar/m2_symbol_grep.txt

Review pack:

- /workspace/tmp/mascar_m2_review_pack_YYYYMMDD_HHMMSS.tar.gz

Include all changed source/config/docs/postcheck/helper files.

Do not commit M2 implementation outputs.

## Stop conditions for M2B

Stop and report if:

1. active owner scheduling requires cache request behavior changes.
2. scheduler deadlocks during smoke and the guard cannot fix it quickly.
3. build fails due to broad unrelated conflicts.
4. implementing compute-first requires unsafe mutation of scoreboard or ibuffer state.
5. M2 begins drifting into re-execution queue or non-owner hit-only behavior.
6. elapsed time exceeds 90 minutes.

## Final report to GPT

Report only:

1. elapsed_sec
2. review pack path
3. git status --short
4. build/smoke status
5. files GPT should review

