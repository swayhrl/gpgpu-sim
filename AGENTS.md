# AGENTS.md — Decoupled-Tag L1 M5 Core Workflow

This repository contains the simulator-core implementation used for M5 mechanism/performance reproduction.

## Mandatory read order on `hrl/decoupled-l1-m5-v0`

1. `docs/dtc_l1/DTC_L1_SPEC.md`
2. `docs/dtc_l1/README.md`
3. Framework `hrl/decoupled-l1-exp-m5-v0:AGENTS.md`
4. Framework `docs/dtc_l1/chatgpt_handoff/CURRENT_STATE.md`
5. Framework `docs/dtc_l1/m5/M5_V1_APPROVAL.md`
6. Framework `docs/dtc_l1/m5/M5_DIRTY_VICTIM_POLICY_RESOLUTION.md`
7. Framework `docs/dtc_l1/m5/M5_V2_GRAPHICS_CONTINUATION_APPROVAL.md`
8. Framework `docs/dtc_l1/m5/M5_EXPERIMENT_MATRIX.md`
9. Framework `docs/dtc_l1/m5/M5_PROBLEM_RESOLUTION_POLICY.md`
10. Framework `docs/dtc_l1/m5/M5_HANDOFF_CONTRACT.md`
11. Framework `docs/dtc_l1/m5/M5_GRAPHICS_PREP.md`
12. Framework `docs/dtc_l1/m5/M5_GRAPHICS_POST_COMPUTE_PLAN.md`
13. Framework `docs/dtc_l1/m5/M5_GRAPHICS_HANDOFF_CONTRACT.md`
14. Framework `docs/dtc_l1/chatgpt_handoff/CODEX_NEXT_STAGE.md`
15. Framework `docs/dtc_l1/chatgpt_handoff/GOAL_START.md`
16. Framework `docs/dtc_l1/codex_handoff/LATEST_REPORT.md`
17. Framework `docs/dtc_l1/implementation/M5_ISSUE_LOG.md`
18. M4 final review pack

## Anchors and branch roles

Validated M1-M4 Core anchor:

`cdeec769fd0c1be12b45d58536ecb81074d4b415`

Active compute M5 Core:

`hrl/decoupled-l1-m5-v0`

Do not modify/back-port M5 work to validated M1-M4.

After M5.6 PASS, record the exact compute-freeze Core SHA and create:

`hrl/decoupled-l1-m5-graphics-v0`

from that SHA. Post-compute graphics frontend/replay integration belongs on the graphics branch so completed compute FORMAL evidence remains immutable.

## Current progression

M5.0A is PASS. M5.0B is active. Close M5-T005 under the approved ratio-zero policy, then continue compute through M5.6.

Compute sequence:

`M5.0B -> M5.0C -> M5.0D -> M5.0E -> M5.1 -> M5.2 -> M5.3 -> M5.4 -> M5.5 -> M5.6`

M5.6 is now a compute freeze boundary, not the persistent Goal terminal state.

After M5.6, continue on graphics branches:

`M5.7 -> M5.8 -> M5.9 -> M5.10 -> M5.11 -> M5.12`

Final M5 state is either `M5_FULL_REPRO_READY_FOR_REVIEW` or, after exhaustive source-backed graphics recovery fails, `M5_COMPUTE_COMPLETE_GRAPHICS_SOURCE_UNAVAILABLE_READY_FOR_REVIEW`.

## Frozen DTC architecture boundary

M5 may repair implementation/modeling fidelity bugs, add source-backed counters, improve workload/graphics integration, and parameterize already-frozen knobs.

Do not silently change frozen DTC semantics to obtain target results. Preserve at least:

- 128B logical Tag->Physical mapping for paper whole-line modes;
- Base PIB=8 / traditional MSHR=32 defaults;
- IO FIFO retirement, PIB 256;
- OO ready retirement, Ref Count/Shadow Ref, PIB 128;
- physical partial allocation/no rollback;
- IO passive release vs OO active reclaim;
- 4 Tag banks, 1 request/bank/cycle, max 4/cycle;
- allocation width 4;
- IO/OO retire width 1/cycle;
- lower issue width 1/SM/cycle;
- no traditional L1 MSHR as DTC capacity/merge state;
- exact fill generation/identity;
- Atomic side effects never merged/lost;
- architectural `.cg` bypass semantics.

`MODERN_OO_SECTOR` remains an extension and must not contaminate paper Figures 4.2-4.10.

## Researcher-frozen compute policy

PAPER_BASE: conventional 16 KiB L1, PIB=8, MSHR=32.

PAPER_IO/PAPER_OO: 16 KiB logical Tag/cache + 80 KiB physical Cacheline Array; IO PIB=256, OO PIB=128.

All paper-facing formal configurations use explicit:

`-gpgpu_l1_cache_write_ratio 0`

Preserve WRITE_THROUGH, allocation, LRU, geometry, timing, MSHR, scoreboard, and DTC semantics otherwise. Ratio 25 is diagnostic platform policy only.

Figure 4.7 common live miss = new-miss lower-request commit -> final lower response; primary metric is per-SM cycle average.

Figure 4.2 paper categories = PIB full, true Tag/cacheline allocation failure, MSHR capacity/merge, miss queue/downstream capacity; Tag-bank arbitration is diagnostic only.

## Problem-resolution behavior

Follow Framework `M5_PROBLEM_RESOLUTION_POLICY.md` and graphics continuation rules.

Do not stop merely for source-backed assertions, missing instrumentation, operation mismatch, weak/negative speedup, absent expected pressure, timeout, build/shader/trace integration failure, or a repairable simulator bug.

Diagnose -> repair -> regress -> invalidate stale evidence -> continue.

Pause only if the next source-correct step changes frozen DTC architecture semantics, requires a proxy to be claimed as formal graphics reproduction, leaves irreducible scientific ambiguity, or reaches a final review state.

## Graphics-specific Core rules

The existing `UNAVAILABLE_WITH_CURRENT_INFRA` result is not permission to special-case graphics inside DTC.

If M5.8 finds a source-backed DIRECT or TRACE path, integrate the frontend/replay path around the validated DTC mechanism. Do not add benchmark/scene IDs, magic addresses, or graphics-specific hit/miss behavior inside DTC.

Texture/fixed-function/framebuffer traffic must be routed according to source evidence and separated when outside DTC scope.

A different graphics timing driver must not be declared comparable to compute cycles without the Framework comparability gate.

## Formal-data and Git discipline

Any behavior/timing Core change invalidates affected downstream FORMAL data on its branch and requires regression.

Compute freeze results remain immutable when graphics branch changes begin.

- Never `git add .` or `git add -A`.
- Stage explicit paths only.
- Keep semantic commits reviewable.
- Do not force-push.
- Preserve diagnostic evidence.
- Record source/config/workload/asset/trace identities.
- Do not commit raw logs/traces/builds/binaries.

Figure 4.6 area/synthesis remains outside M5 and requires separate M6 authorization.
