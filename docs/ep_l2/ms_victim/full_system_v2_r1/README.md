# B full-system V2 incremental delivery r1

Authority: Framework b2e5328aca825d6e9ebfe09e46fa208957834636. Same B Goal,
branch, initial T0 and cumulative campaign; all earlier source/test packs retained.
Runtime source/build is NOT this coordination commit. No C++ compile, native
execution, application start, lease or watcher was created by B in this revision.

A's published 6452b74d92c2cf72f80e5f5e102941d6cf9adf7a is the 949-series dirty/fixed
preimage. The directory, bank identity and fixed-pool helper are byte-identical
to B126bde88. Its cache consumer still omits the already-delivered last-unpin
TVD-role state completion. A645_LAST_UNPIN.patch applies to that exact preimage
in an isolated index; no A worktree or B public file is modified. It changes
no ports, tickets, UID width, pools, packet identity or RO-OFF request routing.
The old dirty_icnt_pinned native test and the new joint case expect the missing
TVD_INFLIGHT->TVD_VALID transition; neither has been compiled or executed.

## Lifecycle and ordering

|Directory / payload stage|Read / pin rule|WB / write / release rule|Evidence|
|---|---|---|---|
|CLEAN / TVD_VALID|All requested valid sectors required; timed directory query then paid bank read; pin until real response enqueue|Missing-sector reclaim only unpinned; write requires promotion and real Tag resources|Existing native cycle/partial tests, unrun|
|WB_STAGED / WB_STAGED|Retained ready sectors can forward through read lease; absent handle or missing sector waits|Immutable dirty snapshot; mandatory row cannot reclaim/promote; accept uses saved row+incarnation+UID, charges accept indexed control|Pending source case, unrun|
|WB_INFLIGHT / WB or TVD_INFLIGHT|Read exact handle+generation; stable client/UID/generation retry; no free response buffer|Lower owns an independent packet snapshot; same-address write waits WB done; row mandatory, pin prohibits transition/reclaim|Existing dirty/pin tests, unrun|
|WB_COMPLETE while reader pinned|Keep exact lease/handle and data; ICNT full cannot consume grant or release pin early|Completion clears directory dirty mask; charges completion CAM+indexed update; payload transition deferred until final enqueue/unpin|A645 missing both-role final state fix; exact patch supplied|
|WB_COMPLETE last unpin / TVD_VALID|Commit reader exactly once, clear pending bank token; preserve returned versions|Both previous WB and TVD roles become TVD_VALID; optional clean retention, promotion or reclaim later|B126 source fix retained, joint input prepared|
|Promotion / RESIDENT_VALID|Same-bank existing handle, correct sector versions|Pre-reserve native Tag/WB/lower credits; reuse source directory row with new incarnation for displaced dirty victim, bind new WB UID; no transient row129|Existing partial/rollback/fixed tests, unrun|
|Epoch / cleanup|RO whole-commandlist guard precedes host or victim mutation|Mandatory WB/readers/pins must drain; no stale-generation completion may touch reused data|Old clean reuse + new joint host/late-fill input, unrun|

All frontend address probes retain the one-cycle, one-query/slice/cycle cost.
A949 local ticket is 96 bits, bound WB UID is32+1 bits. Accept uses its saved
indexed ticket; completion keeps the one-CAM-per-cycle path; three indexed read
views / update lanes remain declared. Observational test getters are not model
functional probes. No extra hash-map admission, directory, payload RAM, descriptor
or response FIFO is introduced. These are source/control costs, NOT RTL PPA.
Atomics remain outside normal application qualification. Existing operation enum
routes a mandatory atomic to WAIT and an optional atomic to conservative native
promotion; no victim READ response may satisfy an atomic. Native negatives remain
required; this delivery claims no atomic numerical execution.

## One shared B/C input, not another cache model

`tests/ep_l2/ms_victim/test_vrw_interleave.cc` reuses the real partition and
B fixture, independent nonzero per-sector version reference and actual lower/
ReturnQ/ICNT packets. Existing geometry defaults remain unchanged; the optional
geometry argument permits C's RO per-address32 setting. Response collection now
checks EVERY requested sector, not only address-selected sector0.

The complete finite footprint, ranges, commandlist identity, configuration and
expected results are frozen in vrw_input/. The synthetic oracle certifies17 exact
RO lines disjoint from144 written lines over this entire bounded fixture epoch.
The refused host update is not an executed write. C scope/implementation review
is PENDING; this is not a real workload trace or a new Conv certificate. Existing
Conv full-epoch oracle is untouched. No local read-only heuristic is substituted.

One sequence covers V-before-R same-address priority; 128 mandatory WAD rows
reclaiming optional clean V; simultaneous native dirty/RO pool use; RO detach,
independent duplicate and partial/out-of-order four-sector return; host refusal
before mutation; actual ICNT capacity1 with reader pinned across WB done; and a
different same-address writer blocked until WB completion then native promotion.
A public RAM request supplies ONE explicitly paid constructed bank contender;
that component injection is not an observed application opportunity. Per-cycle
bank grants<=1, lookup query delta<=1, handle/generation, WB UID, exact-once
response IDs, all-sector data and final pending-token/descriptor drain are asserted.

Expected-only counters are22 real lower read packets/704 bytes of which21/672
are RO (old A0 four sectors plus later duplicate A0 one sector). Descriptor18
asserts common usage, not saturation; C's existing D512 capacity case should be
reused separately. The counts do not establish a comparative traffic benefit,
DRAM issue/timing measurement or SASS numerical correctness. B/C provide inputs;
A alone combines public adapters and E accepts execution. Isolated A645 and
Aaa2 candidates reject this joint mode today; do not remove guards in B or run
this case against an old ABI. A_CMAKE_APPEND_PROPOSAL.txt is un-applied.

## Fixed control boundary and remaining precise integration work

Existing fixed64 helper reserves16 victim slots and276 non-victim slots per bank
(64+1104=1168); unused victim space is not lent out. Directory stays128, descriptor
pool512, banks4x1. All roles other than TVD, including future RO, are counted in
the ordinary bucket; there is only one existing-handle replacement credit and
no fixed physical-id requirement or new RAM. This is a fixed CAPACITY control,
not an address-fixed placement control.

A645 currently rejects fixed+dirty. This is NOT the final V2 W+R+fixed64 control.
Simply dropping that guard is unsafe: retained dirty WB can remain WB-role while
STAGED/accepted-and-pinned and would be counted as ordinary capacity before
transfer to TVD. The one clean replacement credit cannot track arbitrarily many
such owners. A must decide the common accounting delta with B: carry a bounded
existing-handle victim-budget class through retained WB stages (including pin),
precheck/reserve it atomically with the existing row, transfer exactly once and
undo on rollback; do not add a second pool or functional uncharged directory scan.
RO must consume the reciprocal ordinary quota on every allocation and promotion.
Keep the guard until this delta plus native dirty/backpressure/rollback/saturation
cases is accepted. This is an explicit source gap, not a qualified fixed result.

One joint request to A/C identifies this gap, frontend priority/host ordering,
RO/native shared response-head identity, finite four-head arbiter bound, and the
exact input. No public interface is invented in B. A must publish a combined
source and supported profiles; E then supplies current admission, exact objects/
binary/config effective dump and independently accepts affected native cases.
Old21 cases are preserved, not re-run or reclassified by text checks.

Campaign policy is cumulative40, not40 additional starts. E alone adopts it into
the real ledger, preserving used starts; B never resets or edits that ledger.
No resource audit repetition or permanent dependency poller is added.
