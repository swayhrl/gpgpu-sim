# AGENTS.md — Decoupled-Tag L1 Reproduction

This repository participates in the Decoupled-Tag L1 (DTC-L1) reproduction project.

## Mandatory read order

Before modifying source, configuration, tests, or scripts on `hrl/decoupled-l1-v0`, read:

1. `docs/dtc_l1/DTC_L1_SPEC.md` — authoritative frozen architecture specification.
2. `docs/dtc_l1/README.md` — repository role and cross-repository coordination.
3. In `swayhrl/accel-sim-framework@hrl/decoupled-l1-exp-v0`:
   - `docs/dtc_l1/chatgpt_handoff/CURRENT_STATE.md`
   - `docs/dtc_l1/chatgpt_handoff/DISCUSSION_REFERENCE.md`
   - `docs/dtc_l1/chatgpt_handoff/CODEX_NEXT_STAGE.md`

If these disagree, STOP and report the conflict. Do not choose a meaning silently.

## Repository role

This repository is the simulator-core implementation repository. The framework repository is the coordination, experiment-orchestration, and review-pack entry point.

Source anchor for this project branch:

- upstream: `accel-sim/gpgpu-sim_distribution`
- upstream ref: `dev`
- frozen base SHA: `91880c53383d5a6a6742bfb1be2c5f34e39c7871`
- project branch: `hrl/decoupled-l1-v0`

Do not merge or cherry-pick unrelated L1/L2/TLB research branches into this branch unless the current stage specification explicitly authorizes it.

## Research authority and evidence states

Use these labels in design/audit documents:

- `VERIFIED_SOURCE`: directly established from current source/configuration.
- `USER_CONFIRMED`: architecture fact explicitly frozen by the researcher.
- `THESIS_SPEC`: directly specified by the thesis but not yet verified against source.
- `PROVISIONAL_MODEL`: deliberate simulator abstraction chosen for mechanism-level reproduction.
- `UNKNOWN`: not established; must not be guessed.

Never silently upgrade `PROVISIONAL_MODEL` or `UNKNOWN` to a verified fact.

## Scope control

Codex executes only the active `CODEX_NEXT_STAGE.md`. Anything not listed there is out of scope.

In particular:

- Do not implement future stages early.
- Do not refactor unrelated cache, NoC, L2, DRAM, scheduler, or trace code for style.
- Do not change baseline behavior when DTC is disabled.
- Do not add protective behavior that changes the specified mechanism. Protocol/resource invariants belong in assertions and diagnostics, not semantic guards, unless the stage explicitly requires a guard.
- Do not special-case expected results (for example, do not inject a special `16.5KB => deadlock` rule). Deadlock or progress must emerge from the modeled resource dependencies.

## Parameterization requirement

Frozen M0 values are defaults, not hard-coded constants. Architecture-sensitive quantities must be configuration parameters wherever practical, including logical/physical capacity, PIB size, tag banks, allocation width, retire width, coalescer width, lower-request issue width, and outstanding-request limit.

Preset configurations may provide paper defaults, but IO/OO behavior must not be coupled to a fixed PIB size in code.

## Timing and backpressure rules

Unless a later stage explicitly changes this abstraction:

- one pipeline box/stage in the frozen design is one simulator cycle;
- bounded queues/buffers backpressure upstream when full;
- backpressure propagates toward the memory-instruction entrance;
- IO and OO retire width default to one instruction per cycle;
- same-cycle physical-line release is visible to allocation in that cycle;
- L1-to-lower-memory issue width defaults to one request per cycle per SM.

Preserve these rules symmetrically across Baseline/IO/OO comparisons unless the design itself requires a difference.

## Correctness before performance

Every implementation stage must include directed tests for its state transitions before workload experiments.

At minimum, later DTC stages must be able to check invariants such as:

- every valid logical tag maps to an allocated physical line;
- a physical line is never freed while live references remain;
- fills target the intended physical allocation identity;
- a dependency is awakened exactly once;
- `wait_cnt` never underflows;
- IO never retires a head entry whose required data are not ready;
- OO ref-count increment/decrement events are balanced;
- a dynamic memory instruction completes at most once;
- kernel end does not silently discard live PIB/merge/pending state.

## Baseline neutrality

When the DTC backend is disabled, modified code must remain behaviorally and timing neutral relative to the chosen clean baseline, except for explicitly enabled instrumentation. A stage that cannot demonstrate this is not ready for formal experiments.

Use identical trace/input/configuration across Base/IO/OO except for the intended design knobs.

## Git discipline

- Never use `git add .` or `git add -A`.
- Stage explicit paths only.
- Keep commits semantic and reviewable.
- Do not rewrite or force-update shared project branches without explicit authorization.
- Record the base SHA and final SHA for every formal stage.
- Run `git diff --check` and record working-tree status before closeout.

## Artifacts and experiment provenance

Do not commit large raw logs, traces, build directories, or generated binaries.

For formal/diagnostic runs, record at least:

- core SHA;
- framework SHA;
- config identity/SHA;
- trace/workload identity;
- result status: `FORMAL`, `DIAGNOSTIC`, `PRE_FIX`, or `OBSOLETE`.

Use compact review evidence and a raw-log index rather than checking large logs into Git.

## Long-running commands

Do not leave a stalled command indefinitely. If a build/test/experiment shows no meaningful progress, inspect it at roughly 20 minutes, diagnose/escalate at roughly 40 minutes, and stop plus record the state by roughly 60 minutes unless the active stage explicitly expects a longer silent interval.

## STOP boundary

At every stage boundary: implement/audit only the authorized scope, validate, commit, generate evidence, push, report, and STOP. Do not autonomously continue into the next research stage.
