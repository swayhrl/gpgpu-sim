# AGENTS.md — Decoupled-Tag L1 M5 Core Workflow

This repository contains the simulator Core used by the Paper-10 and Extended-20 compute tracks.

## Mandatory read order

1. `docs/dtc_l1/DTC_L1_SPEC.md`
2. `docs/dtc_l1/README.md`
3. Framework active compute `AGENTS.md`
4. Framework `docs/dtc_l1/chatgpt_handoff/CURRENT_STATE.md`
5. Framework `docs/dtc_l1/m5/M5_V3_PARALLEL_TRACKS_APPROVAL.md`
6. Framework `docs/dtc_l1/m5/M5_GRAPHICS_RESEARCH_CLOSEOUT_APPROVAL.md`
7. Framework `docs/dtc_l1/m5/M5_DIRTY_VICTIM_POLICY_RESOLUTION.md`
8. Framework `docs/dtc_l1/m5/M5_V1_APPROVAL.md`
9. Framework `docs/dtc_l1/m5/M5_EXTENDED20_APPROVAL.md`
10. Framework `docs/dtc_l1/m5/M5_EXTENDED20_FORMAL_MATRIX.md`
11. Framework `docs/dtc_l1/m5/M5_PARALLEL_BATCH_POLICY.md`
12. Framework `docs/dtc_l1/m5/M5_PROBLEM_RESOLUTION_POLICY.md`
13. Framework `docs/dtc_l1/m5/M5_HANDOFF_CONTRACT.md`
14. Framework `docs/dtc_l1/m5/M5_EXTENDED20_HANDOFF_CONTRACT.md`
15. Framework `docs/dtc_l1/m5/M5_BRANCH_OWNERSHIP.md`
16. Framework `docs/dtc_l1/chatgpt_handoff/CODEX_NEXT_STAGE.md`
17. Framework `docs/dtc_l1/chatgpt_handoff/GOAL_START.md`
18. Framework `docs/dtc_l1/codex_handoff/LATEST_REPORT.md`
19. Framework `docs/dtc_l1/implementation/M5_ISSUE_LOG.md`

## Branch ownership

Active compute Core:

`hrl/decoupled-l1-m5-v0`

Only the compute Codex window writes this branch/worktree before compute freeze.

The graphics-research window was Framework-only and has now closed at accepted commit:

`hrl/decoupled-l1-exp-m5-graphics-research-v0@ed36abb8f98372dbd1fef11d5b0e8780fb8bf17d`

with status:

`GRAPHICS_SOURCE_BACKED_UNAVAILABLE`

Under current evidence there is no graphics Core integration branch to create after compute freeze. Reopen graphics Core work only if a genuinely new original/source-backed artifact first reopens M5.8 and satisfies the explicit admission contract.

## Current Paper progression

M5.0A is PASS. M5-T005/R5DV is CLOSED. Current work is M5.0B workload recovery.

Continue:

`M5.0B -> M5.0C -> M5.0D -> M5.0E -> M5.1 -> M5.2 -> M5.3 -> M5.4 -> M5.5 -> M5.6`

## Extended-20 progression

Selection is already approved. The compute window also owns:

`M5.E1 -> M5.E2 -> M5.E3`

E1 formalizes source/build/PTX/input/output identities and may progress when host resource conditions allow without disturbing active Paper jobs.

E2's 60 Base/IO/OO primary runs start only after Paper M5.2 freezes the common formal anchor. They may run in parallel with Paper M5.3-M5.6 using the Framework parallel batch policy.

Do not special-case Extended workloads in Core.

## Compute-freeze join barrier

M5.6 alone is not a freeze.

`M5.COMPUTE_FREEZE` requires:

- Paper M5.6 PASS;
- Extended M5.E3 PASS;
- no unresolved correctness/fidelity issue;
- compute Core/Framework branches pushed/clean.

At compute freeze record immutable Core/Framework SHAs. Under the accepted graphics-unavailable closeout, no graphics Core integration follows; Framework proceeds to M5.12 negative-evidence synthesis.

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

## Graphics rule under current closeout

Do not modify Core for graphics under the current accepted `GRAPHICS_SOURCE_BACKED_UNAVAILABLE` result.

Do not add scene IDs, magic addresses, graphics-specific DTC behavior, proxy frontend semantics, or synthetic graphics trace handling merely to produce paper-style graphics bars.

If genuinely new original/source-backed graphics evidence later reopens M5.8, any future graphics integration must still be implemented around the validated DTC mechanism on a fresh branch from the then-recorded compute-freeze Core SHA.

## Git/evidence discipline

- never `git add .` or `git add -A`;
- stage explicit paths only;
- no force-push;
- do not delete diagnostic evidence;
- do not commit raw logs/traces/builds/binaries;
- behavior/timing Core changes require explicit invalidation/regression scope.

Figure 4.6 area/synthesis remains outside M5 and requires M6 authorization.
