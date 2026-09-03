# AGENTS.md — Decoupled-Tag L1 M5 Core Workflow

This repository contains the simulator-core implementation used for M5 mechanism/performance experiments.

## Mandatory read order on `hrl/decoupled-l1-m5-v0`

1. `docs/dtc_l1/DTC_L1_SPEC.md` — frozen architecture specification.
2. `docs/dtc_l1/README.md`.
3. Framework `hrl/decoupled-l1-exp-m5-v0:AGENTS.md`.
4. Framework `docs/dtc_l1/chatgpt_handoff/CURRENT_STATE.md`.
5. Framework `docs/dtc_l1/m5/M5_V1_APPROVAL.md`.
6. Framework `docs/dtc_l1/m5/M5_EXPERIMENT_MATRIX.md`.
7. Framework `docs/dtc_l1/m5/M5_PROBLEM_RESOLUTION_POLICY.md`.
8. Framework `docs/dtc_l1/m5/M5_HANDOFF_CONTRACT.md`.
9. Framework `docs/dtc_l1/m5/M5_GRAPHICS_PREP.md`.
10. Framework `docs/dtc_l1/chatgpt_handoff/CODEX_NEXT_STAGE.md`.
11. Framework `docs/dtc_l1/chatgpt_handoff/GOAL_START.md`.
12. Framework `docs/dtc_l1/codex_handoff/LATEST_REPORT.md`.
13. M4 final review pack as validated historical context.

M5 v1 is ACTIVE. `M5_V1_APPROVAL.md` supersedes the stale planning banner in the long matrix; the matrix body is the approved detailed execution plan.

## Anchors

Parent validated M1-M4 Core:

`cdeec769fd0c1be12b45d58536ecb81074d4b415`

M5 Core branch:

`hrl/decoupled-l1-m5-v0`

Do not modify or back-port M5 work to the validated M1-M4 branch.

## Current continuous progression

The Core may be modified as needed for source-backed M5 fidelity/instrumentation repairs while the Framework Goal runs:

`M5.0A -> M5.0B -> M5.0C -> M5.0D -> M5.0E -> M5.1 -> M5.2 -> M5.3 -> M5.4 -> M5.5 -> M5.6`.

Do not stop for an ordinary repairable Core bug. Follow the Framework problem-resolution policy, regress, invalidate stale results, and resume. Pause only for a real researcher-decision boundary or final `M5_COMPUTE_READY_FOR_REVIEW`.

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

## Researcher-frozen M5 v1 interpretations

### Figure 4.5 main result

PAPER_IO/PAPER_OO primary DTC configuration uses **16KB logical Tag/cache capacity + 80KB physical Cacheline Array**. IO PIB=256; OO PIB=128. PAPER_BASE remains conventional 16KB L1, PIB=8, MSHR=32.

### Figure 4.7 common live misses

Formal M5 uses one lifecycle across Base/IO/OO:

`new miss committed into lower-request ownership -> final lower response`.

Pending-hit merges do not create a new live miss. A distinct duplicate lower request after Tag eviction does.

Primary plotted value is per-SM cycle average:

`sum(live misses across all SMs/cycles) / (num_SM * sampled_kernel_cycles)`.

Do not label MSHR occupancy, NoC inflight occupancy, or pending physical lines as the common Figure-4.7 metric.

### Figure 4.2 stall semantics

Formal paper-equivalent categories are:

- PIB full;
- true Tag/cacheline allocation failure;
- MSHR entry/merge capacity failure;
- miss queue/downstream capacity failure.

Tag-bank arbitration conflict is a separate diagnostic category. Do not silently equate it with Tag/cacheline allocation failure.

## M5 problem resolution

Ordinary implementation issues are solved in-goal rather than immediately stopping. Follow Framework `M5_PROBLEM_RESOLUTION_POLICY.md`.

Do not stop merely for:

- a source-backed assertion under a new M5 workload;
- missing instrumentation;
- operation-count mismatch;
- weak/negative speedup;
- a paper-discussed bottleneck not appearing under the current workload;
- Tag-bank/downstream domination;
- timeout that can be classified;
- a repairable simulator bug.

For a poor performance result, first establish correctness/workload/config identity, then determine whether Base has the intended bottleneck, whether DTC increases common live concurrent misses, whether downstream saturation or duplicate traffic dominates, and whether IO/OO HOL opportunity exists.

Do not tune Core parameters or workload inputs to match thesis speedup numbers.

## Workload/input discipline

Core changes may support canonical workload integration but must not special-case benchmark identities or inputs for performance.

M5.0B explicitly audits `gemv/gemver`, `gesu/gesummv`, and `conv2d/2DConvolution`. Missing binaries are resolved through canonical source/wrappers, not algorithm substitution.

Dataset selection is based on canonical/standard inputs and Base-only full-load/work-amount evidence, never DTC speedup.

## Core-change regression/invalidation

Any M5 Core change that can affect behavior/timing invalidates affected downstream FORMAL results and requires the regression set specified by `M5_PROBLEM_RESOLUTION_POLICY.md`.

Instrumentation-only changes may preserve old performance cycles only after exact sentinel neutrality.

## Git/evidence discipline

- Never `git add .` or `git add -A`.
- Stage explicit paths only.
- Keep semantic commits reviewable.
- Do not force-push.
- Record Core/Framework/config/workload identities.
- Do not commit raw logs/traces/builds/binaries.

Use the Framework handoff contract at every M5 substage. A substage PASS means checkpoint/push/continue, not wait for human confirmation.
