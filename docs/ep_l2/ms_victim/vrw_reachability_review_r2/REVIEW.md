# B: Core27 shared V/R/W reachability and identity review r2

The r35 abort is explained by a test expectation and stimulus reachability error.
It does not demonstrate a lost production lower read. B withdraws the old22/704
and18-descriptor assumptions as runtime test oracles; the original B source,
A8027 typing-fixed source, failed ELF and raw/receipts remain immutable. No
production semantic change is proposed. Core27/main773 stays frozen; E alone
resolves acceptance and holds. This review does not grant independent science
qualification or declare all production paths correct.

## Identity accounting on the exact production source

128B parent U has no separate RO requester descriptor. The sector frontend
allocates four32B children C0..C3, each with its own global mem_fetch UID and
original_mf=U. The partition tracks these children and returns each actual
child through pop(); it does not return a reassembled U here. Each child owns
one descriptor from commonD512: first admits a transaction and other3 merge.
The descriptor holds the exact requester pointer/sector mask and generation.
One payload/transaction pin protects all its waiters; it is not4 data copies.

Each RO lower packet Ls has its OWN UID, RO marker and PayloadHandle/generation,
and NULL original_mf in Core27. It is not Cs or U, and has no new requester
descriptor. Real loweraccept marks issued; real ReturnQ fill uses FILL/Ls UID;
response service uses RESPONSE/Cs UID and common RAM data, then releases that
requester's descriptor exactly once after real enqueue. Restoring the old RO
lower origin pointer would undo the production-origin fix and is prohibited.
The external fixture owns U until four children are actually consumed; requester
replies use actual child UIDs, never invented/copy-assigned UIDs.

WB packets get independent existing UIDs too. A binds that UID to the saved
row/incarnation ticket, preserves its dirty snapshot through real loweraccept
and completion, and checks completion address+UID before mutating row/data.
No RO parent/child UID substitutes for this identity. WB metadata uses the
shared128 WAD/TVD rows and existing bounded WB records, not extra RO descriptors.
Source line anchors for all these statements are in REVIEW.json.

## Why20 is not a missing pair of packets

|Quantity|Old asserted input|Actual abort / source inference|
|---|---|---|
|RO lower packets|4 firstA0 +16 other lines +1 assumed duplicate =21|20 observed:4+16; duplicate adds0lower|
|All lower read packets|1 initial native +21 RO =22|21 only as partial-prefix source inference, not completed result|
|All lower read bytes|704 expected|640 RO bytes observed; initial native32 gives672 partial, no completed delta|
|RO requester descriptors|18 expected as if128B were1requester|21 inferred:4children+16singletons+1duplicate|
|A0 Tag/transaction|Expected detach and newhandle|Observed attached=1,reuse_seen=1,required=issued=15,ready=0,waiters=5|

The denominator difference matters: GDB20 is the RO-only counter, whereas22
was the fixture's final all-read expectation. Three sibling merges set
reuse_seen permanently; can_detach therefore refuses A0 even after another
waiter retires. Repeated A0 finds its attached transaction, merges the fifth
waiter and needs no extra sector because all requested sectors are issued.
The fixture withholds their returns while waiting for an unnecessary packet.
It aborts in submit_ro(0). Its later parentUID, descriptor18 and final count
assertions were not reached and are separate SOURCE findings, not extra observed
native failures. Changing22 to20 alone cannot repair the input or collector.

## WB lifecycle review and existing evidence

|Stage|Frozen production rule|Review evidence / limits|
|---|---|---|
|WB_STAGED|Retain valid sectors for paid read; absent/missing sector waits; mandatory row cannot reclaim/promote|dirty_pending existing producer raw and receipt hash verified, source-disjoint reused evidence, not rerun on Core27|
|WB_INFLIGHT|Actual paid readout+loweraccept; independent WB snapshot; pins can retain WB role|combined_pinned_wb fresh affected Core27 producer evidence, hash verified|
|WB_COMPLETE with reader|One completion CAM/physical cycle + bound UID; row complete; pin prevents early release/role transition|Core27 completion source and retained pinned/ICNT producer evidence|
|Last-unpin|Only after actual response enqueue, reader count--, unpin; WB OR TVD role becomes TVD_VALID|Core27 contains B's both-role fix; dirty_icnt reused raw/receipt verified|
|Same-address write/promotion|Wait mandatory WB and readers, then same-bank native Tag/landing reservation; preserve/replace row incarnation|Source review and original native boundary tests retained|

REUSED_LIFECYCLE_EVIDENCE.json records5 selected raw+receipt verifications,
including fresh combined_same_address/shared_pool/pinned_wb and reused
source-disjoint dirty_pending/dirty_icnt. A's release has20 fresh affected
original cases +4 detailed cases and44 reused source-disjoint cases, not64 fresh
passes. B does not promote producer evidence into E acceptance or replace the
failed additional joint gate with an unrelated passed gate.

## Reachable replacement: two bounded cases with distinct purposes

`split128` retains one actual128B parent plus one32B same-line duplicate, with
all lower returns held until five common descriptors are present. It expects
4 actual lower packets/128B,4 child replies plus1 duplicate reply,0 parent
replies. Return order3,1,2,0 checks partial values and exact UID ownership;
after first three child replies there are2 descriptors left. It does not
expect the merged full-parent transaction to detach. Parent reclamation is
external fixture ownership, not a modeled response assembler.

`joint32` reuses the original initial texture fill, victim priority and144
sector0-written B lines; RO reads A0..A16 are now explicit32B singleton requests.
Once all17 are accepted, the real same-set16Tag constraint forces at least one
unmerged singleton to detach under existing policy. Test observation picks the
smallest actual detached input line; it never assumes A0 or oldest allocation
is selected, because Core27 scans Tag indices. Revisit THAT line with no old
return yet: M=1 cannot find its detached owner in a native LineMSHR index, so a
new handle and one genuine paid lower read are expected. No lower packet is
manufactured to satisfy the old counter.

Expected RO18 packets/576B + initial native1/32B = total19/608B, with18 pending
RO requester descriptors. An old detached return readies a real response at N;
its real enqueue at N+1 fills the actual1-entry ICNT FIFO ahead of the denied
victim retry. ONE explicit paid public-arbiter contender at N supplies constructed
same-bank contention; the fixture includes its grant in physical-cycle totals.
WB completes while reader pin/token remain live; response consumption triggers
last-unpin TVD_VALID. Another same-address writer waits for old WB done and a
legitimate native Tag promotion. Real remaining RO returns/WBs drain, and the
final writer's updated sector is read back. No free response buffer or extra
shared resource is added. The joint case does not saturate D512 or cover every
WB_STAGED interleave; existing C shared_pool and pinned_wb cases remain required.

Inputs, independent whole-synthetic-epoch footprints, hashes, expected results,
CWD and exact argv are in inputs/. No real trace was captured or changed; the
frozen Conv certificate is untouched. C review remains requested/pending; these
are bounded synthetic certificates, never full-application certification.
CORE27_FIXTURE_ONLY.patch adds one test file and applies against exact Core27
in an isolated index. A alone integrates and links it; there is no public source
or CMake edit in B and no change to the frozen producer protocols.

## Actual checks and remaining qualification

Two bounded -fsyntax-only compiler invocations were granted by E's current
content-pinned API (1CPU,1GiB RAM,8MiB result growth each); leases are released.
The first found two local indentation warnings, corrected without changing
operations; the final exact input source passes syntax parsing against Core27
headers. Remaining warnings come from unchanged production/fixture headers.
No object file, link, new producer ELF, simulator execution or full application
was performed by B. Syntax is not functional correctness or reachability PASS.
Original fail and all old review packs remain. Existing E qualifications, actual
40-start budget/T0 and scientific holds are not reset. A/C review then A native
execution with E admission and E independent acceptance remain required.
