# AGENTS.md — Decoupled-Tag L1 M5 Core Workflow

This repository contains the simulator-core implementation used for M5 mechanism/performance experiments.

## Mandatory read order on `hrl/decoupled-l1-m5-v0`

1. `docs/dtc_l1/DTC_L1_SPEC.md` — frozen architecture specification.
2. `docs/dtc_l1/README.md`.
3. Framework `hrl/decoupled-l1-exp-m5-v0:AGENTS.md`.
4. Framework `docs/dtc_l1/chatgpt_handoff/CURRENT_STATE.md`.
5. Framework `docs/dtc_l1/codex_handoff/LATEST_REPORT.md`.
6. Framework `docs/dtc_l1/chatgpt_handoff/CODEX_NEXT_STAGE.md`.
7. Framework `docs/dtc_l1/chatgpt_handoff/GOAL_START.md`.
8. Framework `docs/dtc_l1/m5/M5_EXPERIMENT_MATRIX.md`.
9. Framework `docs/dtc_l1/m5/M5_PROBLEM_RESOLUTION_POLICY.md`.
10. Framework `docs/dtc_l1/m5/M5_HANDOFF_CONTRACT.md`.
11. Framework `docs/dtc_l1/m5/M5_GRAPHICS_PREP.md`.
12. M4 final review pack as validated historical context.

If Framework status is PLANNING HOLD / DRAFT ONLY, do not begin formal M5 implementation or experiments.

## Anchors

Parent validated M1-M4 Core:

`cdeec769fd0c1be12b45d58536ecb81074d4b415`

M5 Core branch:

`hrl/decoupled-l1-m5-v0`

Do not modify or back-port M5 work to the validated M1-M4 branch.

## Frozen architecture boundary

M5 may repair implementation/modeling fidelity bugs discovered by experiments, add source-backed counters, improve workload integration, and parameterize already-frozen knobs.

M5 must not silently change frozen DTC architecture semantics to obtain a target result. Preserve at least:

- 128B logical Tag->Physical mapping for paper whole-line modes;
- paper Base PIB=8 / traditional MSHR=32 defaults;
- IO FIFO retirement, default PIB 256;
- OO ready retirement, Ref Count/Shadow Ref semantics, default PIB 128;
- physical partial allocation/no rollback;
- IO passive release vs OO active reclaim semantics;
- 4 Tag banks, 1 request/bank/cycle, max 4/cycle contract;
- physical allocation width 4;
- IO/OO retire width 1/cycle;
- lower issue width 1/SM/cycle;
- no traditional L1 MSHR as DTC capacity/merge state;
- exact fill generation/identity;
- Atomic side effects never merged/lost;
- architectural `.cg` bypass remains distinct from later optional DTC-native no-Tag policy bypass.

`MODERN_OO_SECTOR` is an extension and must not contaminate primary Figures 4.2-4.10.

## M5 problem resolution

After Goal activation, ordinary implementation issues are solved in-goal rather than immediately stopping. Follow Framework `M5_PROBLEM_RESOLUTION_POLICY.md`.

For a poor performance result, first establish correctness/workload/config identity, then determine whether Base has the intended bottleneck, whether DTC increases common live concurrent misses, whether downstream saturation or duplicate traffic dominates, and whether IO/OO HOL opportunity exists.

Do not tune Core parameters or workload inputs to match thesis speedup numbers.

## Core-change regression/invalidation

Any M5 Core change that can affect behavior/timing invalidates affected downstream FORMAL results and requires the regression set specified by `M5_PROBLEM_RESOLUTION_POLICY.md`.

Instrumentation-only changes may preserve old performance cycles only after exact sentinel neutrality.

## Figure-4.7 common live-miss metric

Formal M5 uses one lifecycle across Base/IO/OO:

`new miss committed into lower-request ownership -> final lower response`.

Pending-hit merges do not create a new live miss. A distinct duplicate request after Tag eviction does.

Do not label MSHR occupancy, NoC inflight occupancy, or pending physical lines as the common Figure-4.7 metric.

## Figure-4.2 stall semantics

Keep separate:

- PIB full;
- true Tag/cacheline allocation failure;
- MSHR entry/merge capacity failure;
- miss queue/downstream capacity failure;
- Tag-bank arbitration conflict as a diagnostic category.

Do not silently equate Tag-bank conflict with Tag/cacheline allocation failure.

## Git/evidence discipline

- Never `git add .` or `git add -A`.
- Stage explicit paths only.
- Keep semantic commits reviewable.
- Do not force-push.
- Record Core/Framework/config/workload identities.
- Do not commit raw logs/traces/builds/binaries.

Use the Framework handoff contract at every M5 substage. A substage PASS means checkpoint/push/continue after M5 Goal authorization, not wait for human confirmation.