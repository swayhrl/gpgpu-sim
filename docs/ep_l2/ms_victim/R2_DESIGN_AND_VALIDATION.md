# R2 source design and pending native validation

Status: SOURCE_REVIEWED_NOT_COMPILED_NOT_RUN. Engineering source delivery,
not a producer release or independent scientific qualification. Clean-first
Core89a75b24 and its adapter_r1/test_plan_r1 bytes are preserved. Historical
be5dcbe1/c197/r14 evidence confers no new qualification.

## Paid clean path

The original1168×128B payload store,1024 native Tags,128 line-MSHRs and512
shared descriptors remain the only data/request storage. The sole128-row
directory replaces WAD membership when enabled, including handle-less mandatory
WB records in CLEAN_ONLY. The inactive legacy WAD address set is not another
functional address directory. Optional clean/completed retained data yields to
mandatory landing/directory demand; pending WB and pinned readers do not.

A native Tag miss starts a victim query. Earliest use is the next physical GPU
cycle, ≤1 victim query/slice/cycle. An incarnation ticket is retained through
retry; repeated attempts inspect the selected row. Read service pins the real
payload, consumes a real P1 bank read, copies actual slot versions into the
native mem_fetch, enqueues in the existing ICNT FIFO, then commits/unpins once.
No spare response carrier or Tag promotion fabricates a victim hit.
Missing sectors use ordinary lower fallback; writes/atomics prepare real Tag/WB
resources before same-handle promotion. A owns final shared metadata query/
update/completion-port accounting; C++ find() is not free CAM hardware evidence.
Host prefill uses exact full-address/quiescent alias-retirement guards. No RO
oracle or trace input is changed.

## Dirty pending/inflight source delta

R1 conservatively waited at WB_STAGED. R2 permits a ready-sector read from the
same paid WB payload before and after lower acceptance. Two live logical reads
may share that handle: the older WB packet readout and the frontend victim read.
The old non-RG1 `(role,id,write)` bank key aliases them. New pure comparison
helper adds client+requestUID+generation ONLY when dirty-TVD is on.
It uses the existing pending/selected scoreboard with a four-native-head bound,
the parent's age ordering, real grants and P1 physical service. RG1 stays OFF;
default OFF and clean-only retain the exact old comparison.

While a victim read is pinned, lower acceptance and set_done advance directory
WB_STAGED→WB_INFLIGHT→WB_COMPLETE but defer the payload's WB→TVD role change.
Its queued key/handle/generation stays stable. Only actual response enqueue
and last unpin finish the role transfer. The WB packet already holds its own
immutable paid lower-FIFO snapshot; no new free read/response buffer is created.
Writes/atomics wait for mandatory WB completion. Unknown dirty sectors retain
WAD-only fallback; partial reads cannot forward fabricated data. Flush/epoch
cleanup refuses outstanding mandatory ownership or pins.

Release cancels unconsumed read/write tokens in dirty mode. Existing UID
cancellation is extended to dirty mode while RG1 counters stay gated to RG1.
This sixth public-file hunk is necessary for rollback/token drain and is a
proposal for A, never a B edit of `ep-l2-rg1.inc`.

## Fixed-capacity partition control — exact definition

`-gpgpu_ep_l2_tvd_fixed_entries 64` reserves **16 victim slots per292-slot bank**;
all other roles share at most276/bank. Total remains1168, split64/1104.
Unused victim quota is NEVER lent to resident/bypass/WB, even with zero actual
victims. Victims cannot exceed their reciprocal quota. With1024 residents full,
combined other-role headroom falls from144 to80. Parent `bypass_capacity()`
remains128 as a role API; it is not a separately guaranteed128-slot allocation
on top of1104. The shared hard cap applies to all non-victim roles.

This is a **fixed bank-capacity quota partition**, with the same paid pointer
mapping, same-bank placement, lookup and data-read path as dynamic retention.
It is not a fixed physical-ID SRAM layout. The r1 note that only address-fixed
IDs and an obligatory copy could constitute any fixed control was an overly
restrictive implementation assumption, not a requirement in EXPERIMENTS.md.
This control fixes capacity in both directions, unlike a victim-count cap
letting other roles borrow unused capacity. A/E must review and label this
exact control; it provides no evidence for a dedicated physical victim array.

One serialized replacement may reserve an exact-handle retention credit: its
old clean resident stays visible to its locked Tag while charged to victim
quota. A real mandatory landing consumes the surrendered non-victim credit.
Transfer to TVD consumes the reservation; rollback first releases the unexposed
landing, restores old ownership and cancels credit. Nothing exceeds1168 or adds
a data carrier. Extra control metadata: one handle/valid plus per-bank quotas/
counts/comparators; implementation derives occupancy from slot metadata, not
data-RAM reads. Hardware counter widths/scheduling/PPA need A's cost review.

Clean victim promotion needs a free non-victim credit or a same-bank clean
resident replacement returning one. A dirty replaced resident remains a paid
WB, so promotion waits for real capacity before changing the directory. Role
changes within the non-victim bucket require no extra quota. Fixed64 is
CLEAN_ONLY; fixed+dirty and local ideals/RG1/MQ2/RO combinations are unqualified.

## Prepared evidence — zero native executions

All18 process entries are in `test_plan_r2/NATIVE_TEST_PLAN.json`. PASS strings,
expected exit21 and numerical assertions are NOT observations.

| Evidence input | Expected distinction | Actual status |
|---|---|---|
| Directory clean/dirty components | finite128 lifecycle, masks, tickets, leases, promotions | NOT_COMPILED_NOT_RUN |
| Native bank legacy/extended | legacy exit21 with merged logical reads; extension drains both with age/P1; UID cancel and generation reuse | NOT_RUN; old failure NOT observed |
| Fixed native allocator | zero victims withholds64 slots; reciprocal64/1104, credit commit/rollback, promotion | NOT_COMPILED_NOT_RUN |
| Primary cache cycle17 OFF/ON | nonzero responses, independent lower packets expected51/17;2 victim responses predicted | NOT_COMPILED_NOT_RUN |
| Stream17 OFF/ON | negative17/17 and zero victim responses | NOT_COMPILED_NOT_RUN |
| Sector3/1/0/2 and missing sector | actual fills, victim version, actual lower fallback, old generation retirement | NOT_COMPILED_NOT_RUN |
| Dirty snapshot | immutable packet, forward, writer waits done then promotion | NOT_COMPILED_NOT_RUN |
| Dirty pending/done while pinned | real two-entry lower FIFO pressure; explicit public-arbiter contender; WB wins first, set_done while reader denied, deferred role/drain | NOT_COMPILED_NOT_RUN |
| Dynamic/fixed64 cache96 | cold96 actual lower reads; reread predicts dynamic+0/fixed+1, correct nonzero return and write promotion | NOT_COMPILED_NOT_RUN |
| Native simple-DRAM clean/dirty | native DRAM issue counters, ICNT/bank pressure, write/atomic labels, partial fallback, WAD full | NOT_COMPILED_NOT_RUN |
| Host-prefill gates | >32-bit exact alias, response/dirty refusal atomicity, unrelated row survives | NOT_COMPILED_NOT_RUN |

The external-memory primary cache fixture is an independent version producer
and actual lower-packet observer, not native DRAM timing or a byte-accurate GPU
memory system. Separate native simple-DRAM fixtures check actual DRAM issue
counts using version-zero lower memory. Neither is numerical SASS atomic/
partial-byte arithmetic proof. served_sector_bytes is not avoided DRAM bytes;
altered later hit trajectories can magnify per-victim service.

Remaining risks: A's metadata ports and newer diagnostic guards/observer hooks;
packet/reference/epoch after generation reuse; pending reader with ICNT-full
during grant retention; rollback at every real cache refusal; combined mandatory
directory/slot/bank starvation; default-OFF equivalence on final same-source
objects. These have not run, and18 processes are not exhaustive closure.

No new compiler, native process, full app, lease or watcher ran. Initial
workspace10277838848B was below51539607552B; historical resource evidence is
not fresh admission. E's legal new-campaign admission and A-integrated source/
ABI are required. B does not lower floors, borrow other projects' resources or
start E's full applications.
