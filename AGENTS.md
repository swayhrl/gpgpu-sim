# AGENTS.md — Decoupled-Tag L1 Reproduction

This repository is the simulator-core implementation repository for the Decoupled-Tag L1 (DTC-L1) reproduction project.

## Mandatory read order

Before modifying source/config/tests on `hrl/decoupled-l1-m1m4-v0`, read:

1. `docs/dtc_l1/DTC_L1_SPEC.md` — frozen architecture specification.
2. `docs/dtc_l1/README.md`.
3. In `swayhrl/accel-sim-framework@hrl/decoupled-l1-exp-m1m4-v0`:
   - `AGENTS.md`
   - `docs/dtc_l1/chatgpt_handoff/CURRENT_STATE.md`
   - `docs/dtc_l1/codex_handoff/LATEST_REPORT.md`
   - `docs/dtc_l1/chatgpt_handoff/CODEX_NEXT_STAGE.md`
   - `docs/dtc_l1/chatgpt_handoff/GOAL_START.md`
   - `docs/dtc_l1/goal/M4_FENCE_REACHABILITY_RESOLUTION.md`
   - `docs/dtc_l1/implementation/M4_MEMORY_OP_SEMANTICS.md`
   - `docs/dtc_l1/goal/VALIDATION_ACCEPTANCE_MATRIX.md`
   - `docs/dtc_l1/goal/M1_M4_GOAL_PLAN.md`
   - `docs/dtc_l1/goal/COUNTER_INVARIANT_SPEC.md`

For M4 fence-related requirements only, the specific fence-reachability resolution plus the updated validation matrix intentionally refine older generic plan language. For any other disagreement, STOP and report.

## Branch/source anchors

Frozen M0 core anchor:

- upstream `accel-sim/gpgpu-sim_distribution:dev` at `91880c53383d5a6a6742bfb1be2c5f34e39c7871`;
- M0 branch `hrl/decoupled-l1-v0`.

Active implementation branch: `hrl/decoupled-l1-m1m4-v0`.

Do not modify M0 or merge unrelated L1/L2/TLB research branches.

## Evidence states

Use:

- `VERIFIED_SOURCE`
- `USER_CONFIRMED`
- `THESIS_SPEC`
- `PROVISIONAL_MODEL`
- `UNKNOWN`
- `SOURCE_UNREACHABLE_NA` only when the active framework specification explicitly authorizes it from source audit.

Never guess or silently upgrade uncertainty.

## Current persistent Goal

M1, M2, and M3 are closed PASS. Finish M4 only, then STOP at `READY_FOR_M5_REVIEW`.

Ordinary progress is not a stop condition. Continue after successful tests/builds/workloads/checkpoints once safe evidence is committed/pushed.

STOP on an active HARD failure, a source-reachable semantic ambiguity requiring human judgment, regression of M1-M3, need to change frozen M0 semantics, or final M4 completion.

M5 is not authorized.

## M4 source-reachability boundary

The current PTX frontend has been verified unable to generate the existing dynamic `FENCE_OP` / async proxy-fence path. This is a source limitation, not permission to add unrelated frontend semantics.

Do NOT:

- add `fence` lexer/parser/static-decode support in M4;
- map `membar` to `FENCE_OP`/proxy fence;
- force `set_proxy_fence()` or `set_fence_proxy_kind()` on ordinary instructions;
- suppress the source's unsupported regular-fence behavior to make old tests execute.

Use F00A-F00D and the `SOURCE_UNREACHABLE_NA` disposition for F01-F03 exactly as specified in the framework resolution file.

If a real source-backed PTX path is discovered that produces `FENCE_OP`, STOP and reopen fence validation before continuing.

## Core architecture discipline

- Do not change LEGACY or closed M1-M3 behavior.
- Do not use traditional L1 MSHR as DTC capacity/merge state.
- Do not fabricate conventional `m_extra_mf_fields`/MSHR state for Paper-IO reads.
- Do not route IO-owned reads through conventional `baseline_cache::fill()`.
- Do not keep DTC and conventional L1D read backends active for the same request.
- Preserve 128B Tag->Physical semantics, partial allocation/no rollback, finite widths, and configured backpressure.
- Atomic side effects must never be merged/lost by read Pending-hit logic.
- Preserve architectural bypass; thesis policy bypass remains out of scope.
- Do not alter L2/NoC/DRAM for DTC benefit in M4.
- Do not tune to thesis speedup values.

## Parameterization and timing

Frozen values are defaults, not magic constants. Keep configuration controls for mode, logical/physical capacity, line/sector geometry, PIB, Tag banks/throughput, allocation/retire/coalescer widths, lower issue/outstanding, Ref Count, and debug/watchdog controls.

One frozen pipeline box is one simulator cycle. Bounded structures backpressure upstream. IO/OO retire width defaults to 1/cycle. Per-SM lower issue defaults to 1/cycle. Physical partial allocations are retained; no rollback. Same-cycle release visibility is preserved.

## Required invariant families

Preserve/assert as applicable:

- PIB accounting/capacity;
- valid Tag -> allocated physical line;
- physical generation/fill identity;
- no stale fills;
- exact wakeup/dependency closure;
- IO head-only ready retirement;
- OO Ref Count == Shadow Ref in checker mode;
- reclaim only at `tag_valid==0 && ref_count==0`;
- 128B Tag->Physical under sector readiness;
- completion at most once;
- lower credit/issue-width closure;
- Atomic side effects not merged/lost;
- source-reachable operation counts/drain closure;
- if a real FENCE_OP producer appears, required ordering must be revalidated before acceptance.

## Git/evidence discipline

- Never `git add .` or `git add -A`; stage explicit paths.
- Keep semantic commits reviewable.
- Do not force-push.
- Record Core/Framework/config/workload identities.
- Run `git diff --check` and record clean status at closeout.
- Do not commit raw traces, large logs, build directories, or binaries.

M4 workload results are diagnostic bring-up evidence, not final paper performance evidence.

## Long-running jobs

Inspect no-progress jobs around 20 minutes, diagnose/escalate around 40 minutes, and stop/record around 60 minutes unless longer silence is explicitly expected. A job with measurable progress is not a stop condition.

## Final STOP boundary

After all active M4 HARD gates pass, create/push the M4 review pack, update Framework `LATEST_REPORT.md` to `READY_FOR_M5_REVIEW`, ensure both worktrees are clean, and STOP. Do not begin M5.
