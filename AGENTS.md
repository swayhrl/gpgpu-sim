# AGENTS.md — Decoupled-Tag L1 Reproduction

This repository is the simulator-core implementation repository for the Decoupled-Tag L1 (DTC-L1) reproduction project.

## Mandatory read order

Before modifying source, configuration, tests, or scripts on `hrl/decoupled-l1-m1m4-v0`, read:

1. `docs/dtc_l1/DTC_L1_SPEC.md` — authoritative frozen architecture specification.
2. `docs/dtc_l1/README.md` — repository role and cross-repository coordination.
3. In `swayhrl/accel-sim-framework@hrl/decoupled-l1-exp-m1m4-v0`:
   - `AGENTS.md`
   - `docs/dtc_l1/chatgpt_handoff/CURRENT_STATE.md`
   - `docs/dtc_l1/codex_handoff/LATEST_REPORT.md`
   - `docs/dtc_l1/chatgpt_handoff/CODEX_NEXT_STAGE.md`
   - `docs/dtc_l1/chatgpt_handoff/GOAL_START.md`
   - `docs/dtc_l1/chatgpt_handoff/DISCUSSION_REFERENCE.md`
   - `docs/dtc_l1/goal/M2_IO_RESPONSE_RECOVERY_SPEC.md`
   - `docs/dtc_l1/goal/M1_M4_GOAL_PLAN.md`
   - `docs/dtc_l1/goal/COUNTER_INVARIANT_SPEC.md`
   - `docs/dtc_l1/goal/VALIDATION_ACCEPTANCE_MATRIX.md`

If these disagree, STOP and report the conflict. Do not choose a meaning silently.

## Repository role and branch anchors

Frozen M0 core anchor:

- upstream: `accel-sim/gpgpu-sim_distribution:dev`
- upstream frozen base SHA: `91880c53383d5a6a6742bfb1be2c5f34e39c7871`
- M0 branch: `hrl/decoupled-l1-v0`
- M0 documentation SHA: `5e35de9914f1ad28647ef3a416d054b86f3e44a5`

Active continuous-goal branch:

- `hrl/decoupled-l1-m1m4-v0`

The M0 branch is a read-only design anchor. Do not implement or back-port M1-M4 into it during this goal.

Do not merge or cherry-pick unrelated L1/L2/TLB research branches into the active DTC branch unless the current goal explicitly authorizes it.

## Research authority and evidence states

Use these labels in design/audit documents:

- `VERIFIED_SOURCE`: directly established from current source/configuration.
- `USER_CONFIRMED`: architecture fact explicitly frozen by the researcher.
- `THESIS_SPEC`: directly specified by the thesis but not yet verified against source.
- `PROVISIONAL_MODEL`: deliberate simulator abstraction chosen for mechanism-level reproduction.
- `UNKNOWN`: not established; must not be guessed.

Never silently upgrade `PROVISIONAL_MODEL` or `UNKNOWN` to a verified fact.

## Scope control and persistent Goal mode

M1 Foundation is already closed PASS. The active persistent Goal is the remaining M2 work through M4. The durable objective and stopping condition are in framework `docs/dtc_l1/chatgpt_handoff/GOAL_START.md`.

Codex may continue M2 -> M3 -> M4 without human confirmation only when every HARD gate for the completed stage passes and the required review evidence/commits have been created and pushed.

Ordinary progress is not a stop condition. Do not stop merely because a directed test passed, a safe semantic checkpoint was committed/pushed, a build succeeded, or M2/M3 passed. Continue toward the persistent Goal after preserving the required evidence.

If any HARD gate fails, or source audit exposes a semantic ambiguity that would require guessing, STOP and report. Do not continue into later stages hoping they will mask the issue.

M5 and later work is not authorized.

In particular:

- Do not refactor unrelated cache, NoC, L2, DRAM, scheduler, or trace code for style.
- Do not change `LEGACY` behavior when project mechanisms are disabled or regress the closed M1 boundary.
- Do not add protective behavior that changes the specified mechanism. Protocol/resource invariants belong in assertions and diagnostics, not semantic guards, unless the goal explicitly requires a guard.
- Do not special-case expected results such as `16.5KB => deadlock`; progress/deadlock must emerge from modeled resource dependencies.
- Do not tune implementation to hit thesis speedup numbers.
- Do not use the traditional L1 MSHR as DTC's own capacity/merge mechanism.
- Do not fabricate conventional `m_extra_mf_fields`/MSHR state to make Paper-IO read responses fit `baseline_cache::fill()`.
- Do not route IO-owned read responses through conventional `baseline_cache::fill()`.
- Do not keep DTC and conventional L1D read backends active for the same Paper-IO request.
- Do not merge Atomic side effects through read Pending-hit merge logic.
- Do not implement thesis DTC policy bypass in M1-M4.
- Do not start sector extension until whole-line OO HARD gates pass.

## Parameterization requirement

Frozen M0 values are defaults, not hard-coded constants. Architecture-sensitive quantities must be configuration parameters wherever practical, including:

- mode;
- logical/physical capacity;
- line/sector geometry;
- PIB size;
- Tag banks and per-bank throughput;
- allocation width/policy;
- retire width/policy;
- coalescer width;
- lower-request issue width;
- global outstanding limit;
- Ref Count width;
- debug/stat/watchdog controls.

Preset configurations may provide paper defaults, but mode behavior must not depend on magic capacities.

## Timing and backpressure rules

Unless an authorized later specification changes the abstraction:

- one pipeline box/stage in the frozen design is one simulator cycle;
- bounded queues/buffers backpressure upstream when full;
- backpressure propagates toward the memory-instruction entrance;
- IO and OO retire width default to one instruction per cycle;
- same-cycle physical-line release is visible to allocation in that cycle;
- each SM's L1-to-lower-memory issue width defaults to one request/cycle;
- physical partial allocation is retained on stall; no rollback.

## Correctness before performance

Every mechanism requires directed state-transition tests before workload experiments.

Required invariant families include:

- explicit PIB capacity/accounting;
- valid Tag -> allocated physical line;
- fill -> intended physical allocation identity;
- no stale fill after physical reuse;
- dependency wakeup exactly once;
- `wait_cnt` nonnegative;
- IO head-only retirement with data ready;
- OO Ref Count/Shadow Ref agreement;
- reclaim only when `tag_valid==0 && ref_count==0`;
- sector readiness without changing 128B Tag->Physical mapping;
- dynamic instruction completion at most once;
- lower-request credit/issue-width accounting;
- Atomic side effects not merged/lost;
- required Fence ordering preserved.

## Baseline neutrality

Maintain a project `LEGACY` path exactly equivalent to the frozen clean upstream baseline when paper/DTC features are disabled. M1 already validated this; later stages must not regress it.

Use identical trace/input/unrelated configuration across Paper Base/IO/OO.

## Observability

The project is not complete if it only reports cycles/speedup. Implement the counter, latency, HOL, MLP, physical-allocation, Ref/Merge, and operation-type observability required by the framework goal specs.

Provide bounded/filterable debug event tracing rather than unbounded per-request logs.

## Git discipline

- Never use `git add .` or `git add -A`.
- Stage explicit paths only.
- Keep commits semantic and reviewable.
- Do not rewrite or force-update shared project branches without explicit authorization.
- Record base/final Core and Framework SHAs for every stage/run.
- Run `git diff --check` and record working-tree status before closeout.

## Artifacts and experiment provenance

Do not commit large raw logs, traces, build directories, or generated binaries.

For formal/diagnostic runs, record at least:

- Core SHA;
- Framework SHA;
- config identity/SHA;
- trace/workload identity;
- result status.

Use compact review evidence and raw-log indexes.

## Long-running commands

If a build/test/experiment shows no meaningful progress, inspect around 20 minutes, diagnose/escalate around 40 minutes, and stop plus record state by around 60 minutes unless the active goal explicitly expects a longer silent interval. A long job with measurable progress is not a stop condition.

## Final STOP boundary

Within the authorized M2-M4 Goal, passing a major-stage HARD gate permits automatic continuation after evidence/commit/push.

After M4 passes, push final evidence, update the framework Codex handoff to `READY_FOR_M5_REVIEW`, and STOP. Do not begin M5.
