# AGENTS.md — Decoupled-Tag L1 M5 Core Workflow

This repository contains the simulator-core implementation used for M5 mechanism/performance experiments.

## Mandatory read order on `hrl/decoupled-l1-m5-v0`

1. `docs/dtc_l1/DTC_L1_SPEC.md` — frozen architecture specification.
2. `docs/dtc_l1/README.md`.
3. Framework `hrl/decoupled-l1-exp-m5-v0:AGENTS.md`.
4. Framework `docs/dtc_l1/chatgpt_handoff/CURRENT_STATE.md`.
5. Framework `docs/dtc_l1/m5/M5_V1_APPROVAL.md`.
6. Framework `docs/dtc_l1/m5/M5_DIRTY_VICTIM_POLICY_RESOLUTION.md`.
7. Framework `docs/dtc_l1/m5/M5_EXPERIMENT_MATRIX.md`.
8. Framework `docs/dtc_l1/m5/M5_PROBLEM_RESOLUTION_POLICY.md`.
9. Framework `docs/dtc_l1/m5/M5_HANDOFF_CONTRACT.md`.
10. Framework `docs/dtc_l1/m5/M5_GRAPHICS_PREP.md`.
11. Framework `docs/dtc_l1/chatgpt_handoff/CODEX_NEXT_STAGE.md`.
12. Framework `docs/dtc_l1/chatgpt_handoff/GOAL_START.md`.
13. Framework `docs/dtc_l1/codex_handoff/LATEST_REPORT.md`.
14. Framework `docs/dtc_l1/implementation/M5_ISSUE_LOG.md`.
15. M4 final review pack as validated historical context.

`M5_DIRTY_VICTIM_POLICY_RESOLUTION.md` is the researcher-approved specific refinement for M5-T005.

## Anchors

Parent validated M1-M4 Core:

`cdeec769fd0c1be12b45d58536ecb81074d4b415`

M5 Core branch:

`hrl/decoupled-l1-m5-v0`

Do not modify/back-port M5 work to the validated M1-M4 branch.

## Current progression

M5.0A is PASS. M5.0B is active.

M5-T005's researcher decision is resolved. The formal conventional-L1 policy is explicit `-gpgpu_l1_cache_write_ratio 0` for all paper-facing modes/config variants.

Execute the Framework R5DV sequence, close M5-T005, then continue M5.0B and the remaining M5 sequence automatically.

Ordinary repairable Core issues remain in-goal problems. Pause only at a new genuine researcher-decision boundary or terminal `M5_COMPUTE_READY_FOR_REVIEW`.

## Frozen architecture boundary

M5 may repair implementation/modeling fidelity bugs, add source-backed counters, improve workload integration, and parameterize already-frozen knobs.

M5 must not silently change frozen DTC architecture semantics to obtain a target result. Preserve at least:

- 128B logical Tag->Physical mapping for paper whole-line modes;
- paper Base PIB=8 / traditional MSHR=32 defaults;
- IO FIFO retirement, default PIB 256;
- OO ready retirement, Ref Count/Shadow Ref semantics, default PIB 128;
- physical partial allocation/no rollback;
- IO passive release vs OO active reclaim semantics;
- 4 Tag banks, 1 request/bank/cycle, max 4/cycle;
- physical allocation width 4;
- IO/OO retire width 1/cycle;
- lower issue width 1/SM/cycle;
- no traditional L1 MSHR as DTC capacity/merge state;
- exact fill generation/identity;
- Atomic side effects never merged/lost;
- architectural `.cg` bypass distinct from optional DTC-native no-Tag policy bypass.

`MODERN_OO_SECTOR` is an extension and must not contaminate Figures 4.2-4.10.

## Researcher-frozen M5 interpretations

### Figure 4.5

PAPER_IO/PAPER_OO primary DTC configuration uses 16 KiB logical Tag/cache capacity + 80 KiB physical Cacheline Array. IO PIB=256; OO PIB=128. PAPER_BASE remains conventional 16 KiB L1, PIB=8, MSHR=32.

### Conventional-L1 dirty-victim policy

For every paper-facing formal configuration use explicitly:

`-gpgpu_l1_cache_write_ratio 0`.

Keep write policy `WRITE_THROUGH`, write-allocation semantics, LRU, geometry, timing, MSHR, and scoreboard semantics otherwise unchanged.

Do **not** modify `tag_array::probe` to add a new starvation fallback solely to preserve ratio 25. The inherited ratio 25 is diagnostic platform policy only.

A directed real-path regression must prove that a set whose ways are locally MODIFIED can legally replace a victim and make forward progress under ratio 0 without weakening assertions or fabricating writeback semantics.

### Figure 4.7

Common live miss: new-miss lower-request commit -> final lower response. Primary plotted metric is per-SM cycle average.

### Figure 4.2

Formal categories: PIB full; true Tag/cacheline allocation failure; MSHR entry/merge failure; miss queue/downstream capacity failure. Tag-bank arbitration is diagnostic only.

## M5 problem resolution

Follow Framework `M5_PROBLEM_RESOLUTION_POLICY.md`.

Do not stop merely for a source-backed assertion, missing instrumentation, operation mismatch, weak/negative speedup, absent expected pressure, Tag/downstream domination, timeout, repairable simulator bug, or a performance shift after ratio 0.

For poor performance, establish correctness/workload/config identity first, then analyze Base bottleneck, DTC live concurrency, downstream saturation, duplicate traffic, and IO/OO HOL opportunity.

Do not tune Core parameters/workload inputs to match thesis speedups.

## Formal-data and regression discipline

Any behavior/timing Core change invalidates affected downstream FORMAL results and requires the M5 regression set.

The ratio-0 decision is a formal configuration identity change. Ratio-25 runs remain diagnostic and cannot be relabeled as formal ratio-0 results.

Instrumentation-only changes may preserve old performance cycles only after exact neutrality differential.

## Git/evidence discipline

- Never `git add .` or `git add -A`.
- Stage explicit paths only.
- Keep semantic commits reviewable.
- Do not force-push.
- Preserve ratio-25 diagnostic evidence.
- Record Core/Framework/config/workload identities.
- Do not commit raw logs/traces/builds/binaries.

Use the Framework handoff contract at every M5 substage. A substage PASS means checkpoint/push/continue, not wait for human confirmation.