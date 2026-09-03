# AGENTS.md — Decoupled-Tag L1 M5 Core Workflow

This repository contains the simulator Core used by the Paper-10 and Extended-20 compute tracks.

## Mandatory read order

1. `docs/dtc_l1/DTC_L1_SPEC.md`
2. `docs/dtc_l1/README.md`
3. Framework active compute `AGENTS.md`
4. Framework `docs/dtc_l1/chatgpt_handoff/CURRENT_STATE.md`
5. Framework `docs/dtc_l1/m5/M5_V3_PARALLEL_TRACKS_APPROVAL.md`
6. Framework `docs/dtc_l1/m5/M5_DIRTY_VICTIM_POLICY_RESOLUTION.md`
7. Framework `docs/dtc_l1/m5/M5_V1_APPROVAL.md`
8. Framework `docs/dtc_l1/m5/M5_EXTENDED20_APPROVAL.md`
9. Framework `docs/dtc_l1/m5/M5_EXTENDED20_FORMAL_MATRIX.md`
10. Framework `docs/dtc_l1/m5/M5_PARALLEL_BATCH_POLICY.md`
11. Framework `docs/dtc_l1/m5/M5_PROBLEM_RESOLUTION_POLICY.md`
12. Framework `docs/dtc_l1/m5/M5_HANDOFF_CONTRACT.md`
13. Framework `docs/dtc_l1/m5/M5_EXTENDED20_HANDOFF_CONTRACT.md`
14. Framework `docs/dtc_l1/m5/M5_BRANCH_OWNERSHIP.md`
15. Framework `docs/dtc_l1/m5/M5_GRAPHICS_INDEPENDENT_WINDOW_HANDOFF.md`
16. Framework `docs/dtc_l1/m5/M5_GRAPHICS_HANDOFF_CONTRACT.md`
17. Framework `docs/dtc_l1/chatgpt_handoff/CODEX_NEXT_STAGE.md`
18. Framework `docs/dtc_l1/chatgpt_handoff/GOAL_START.md`
19. Framework `docs/dtc_l1/codex_handoff/LATEST_REPORT.md`
20. Framework `docs/dtc_l1/implementation/M5_ISSUE_LOG.md`

## Branch ownership

Active compute Core:

`hrl/decoupled-l1-m5-v0`

Only the compute Codex window writes this branch/worktree before compute freeze.

The separate graphics-research window is Framework-only and must not modify Core during M5.7/M5.8.

Fresh graphics Core branch:

`hrl/decoupled-l1-m5-graphics-v0`

is created only after `M5.COMPUTE_FREEZE`, from exact `COMPUTE_FREEZE_CORE_SHA`.

## Current Paper progression

M5.0A is PASS. M5.0B/R5DV remains active until the approved ratio-zero canonical SpMV validation closes M5-T005.

Then continue:

`M5.0B -> M5.0C -> M5.0D -> M5.0E -> M5.1 -> M5.2 -> M5.3 -> M5.4 -> M5.5 -> M5.6`

## Extended-20 progression

Selection is already approved. The compute window also owns:

`M5.E1 -> M5.E2 -> M5.E3`

E1 formalizes source/build/PTX/input/output identities.

E2's 60 Base/IO/OO primary runs start only after Paper M5.2 freezes the common formal anchor. They may run in parallel with Paper M5.3-M5.6 using the Framework parallel batch policy.

Do not special-case Extended workloads in Core.

## Compute-freeze join barrier

M5.6 alone is not a freeze.

`M5.COMPUTE_FREEZE` requires:

- Paper M5.6 PASS;
- Extended M5.E3 PASS;
- no unresolved correctness/fidelity issue;
- compute Core/Framework branches pushed/clean.

Only then record immutable compute freeze SHAs and allow graphics Core integration.

## Frozen DTC architecture boundary

Do not change frozen DTC semantics to obtain target speedups. Preserve at least:

- 128B logical Tag->Physical mapping for paper whole-line modes;
- Base PIB=8 / traditional MSHR=32;
- IO FIFO retirement, PIB=256;
- OO ready retirement, Ref Count/Shadow Ref, PIB=128;
- partial physical allocation/no rollback;
- IO passive release vs OO active reclaim;
- 4 Tag banks, 1 request/bank/cycle, max 4/cycle;
- allocation width 4;
- IO/OO retire width 1/cycle;
- lower issue width 1/SM/cycle;
- no traditional L1 MSHR as DTC capacity/merge state;
- exact fill generation/identity;
- Atomic side effects never merged/lost;
- architectural `.cg` bypass semantics.

`MODERN_OO_SECTOR` remains outside paper Figures 4.2-4.10.

## Frozen compute policy

PAPER_BASE: conventional 16 KiB, 128B, 4-way, PIB=8, MSHR=32.

PAPER_IO/PAPER_OO: 16 KiB logical Tag + 80 KiB physical array; IO PIB=256, OO PIB=128.

All formal M5 configs use:

`-gpgpu_l1_cache_write_ratio 0`

Ratio 25 is diagnostic only.

Figure 4.7 common live miss = new-miss lower-request commit -> final lower response; primary metric = per-SM cycle average.

Figure 4.2 categories = PIB full, true Tag/cacheline allocation failure, MSHR capacity/merge, miss queue/downstream capacity; Tag-bank arbitration is diagnostic.

## Problem-resolution behavior

Ordinary assertions, instrumentation gaps, workload mismatches, timeouts, weak/negative speedups and repairable Core bugs are resolve-in-goal.

Reproduce -> classify -> repair -> regress -> invalidate affected formal identities -> continue.

Do not tune Core/workload parameters to thesis bars.

## Graphics rule

M5.7/M5.8 research does not touch this Core.

If source-backed graphics is found, M5.9+ integrates around the validated DTC mechanism only on the post-freeze graphics Core branch. No scene IDs, magic addresses, or graphics-specific DTC behavior.

## Git/evidence discipline

- never `git add .` or `git add -A`;
- stage explicit paths only;
- no force-push;
- do not delete diagnostic evidence;
- do not commit raw logs/traces/builds/binaries;
- behavior/timing Core changes require explicit invalidation/regression scope.

Figure 4.6 area/synthesis remains outside M5 and requires M6 authorization.
