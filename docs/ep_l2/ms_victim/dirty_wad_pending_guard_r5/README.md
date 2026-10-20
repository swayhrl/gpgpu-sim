# B actual dirty WAD retry counterexample and minimal public proposal, r5

After publishing actual clean functionality r4, B ran A's ALREADY FROZEN test
ELF cadf8d519... with --study-dirty-inflight, under E test admission. No newbuild,
main, source edit or application was started. Runtime c994c893/fixture0e4fd51e,
Framework9dd1ee57/vendorf90c430c provenance is retained, not retagged as Core27.
The relevant five common source files are byte-identical to Core27.

## Actual outcomes; no group PASS

The first three native subcases passed: dirty retained forwarding of the
upstream writer's nonzero UID+1 sector version while WB remains INFLIGHT;
same-address writer and conservative native atomic promotion wait for real WB
completion then preserve the handle and check the new version; missing-sector
read waits for completion then uses a real lower fallback. Atomic arithmetic
or application qualification is not claimed. The fourth case failed SIGABRT
at ep-l2-tvd-directory.h:157 !live(find(line)). All positive/negative raw and
receipts remain. The full dirty group is NOT PASS.

One bounded read-only GDB replay caught the same SIGABRT. Stack places it in
l2_cache::preview_access line2754, invoked during dirty_wad_full drain atline382
AFTER release of the lower hold. Thus WAD128 occupied/mandatory and16stable
blocked-cycle assertions had already succeeded. This is not merely a prepared
or mocked source counterexample. GDBroot exits0 after capture/quit; its failed
inferior is not a nativePASS. Exact root/inferior PID/start/boot and raw SHA are
retained. Two r5 E engineering leases are released.

## Observed state and source diagnosis

Current mf UID149 equals m_ft_pending.request UID149. pending.dirty=1,wad=1,
bound=0,tag_index0,victim_address16384, victimH512:g1, saved wad_ticket index0 /
incarnation2. Template `this` was optimized out, so no exact directory row dump
is invented. The assertion itself proves a matching live address row; its
relation to the observed own pending reservation follows the production source.

A pending replacement already reserved that WAD row before its actual bank
write/Tag/WB commit. A bank-denied retry still sees the old dirty Tag owner.
The preview condition currently evaluates new-owner preview_install(...,true,
true) BEFORE the final !(ft_enabled && pending.request==mf && pending.wad).
The uniqueness precondition is invalid for this own pending reservation, so
assertion aborts before the intended ownership guard can skip the new-row check.
This does not demonstrate a second independent owner or actual data corruption.
It demonstrates an evaluated precondition/order bug with assertions enabled.

A_CORE27_OWN_PENDING_GUARD_ORDER_PROPOSAL.patch moves the EXISTING guard before
that preview. It adds no owner credit, WAD row, query/port, descriptor, data,
response buffer or packet. New requests still do the same capacity/uniqueness
check; an own retry uses its already reserved row and later normal bind/commit/
rollback. This is a proposed preservation of intended retry semantics, not a
scientific policy or resource-budget change. Exact-Core27 isolated-index patch
check passes. No B/A public source was edited, no new production/postfix binary
exists, and no red->green result is claimed. A owns public integration/review.

The unmodified --study-dirty-inflight is the production regression: the old
frozen ELF gives actual SIGABRT; fixed A source must reach the fourth terminal
and final drain. A/E should reuse affected OFF, new-request/full,rollback,
bankretry,WB UID,generation/descriptor/ROmux gates, without repeating unaffected
passes as new evidence. E alone determines current profile holds/qualification;
this is not an actual Core27/main773 application execution or unilateral
revocation. Current shared-source risk is explicit; originalfailedcase stays.

At r5, B has4 native invocations:2 clean successes (direct+read-only diagnostic)
and2 dirty failures (direct+captured diagnostic), plus2 earlier debugger startup
failures with no inferior. There are0 builds/links/apps. R4 clean functional
scope stays valid at its recorded legacy RO-OFF boundary. New current-source
joint r3 and full fixeddirtyRO remain unqualified. Broad original Goal is not
complete. Stop the affected dirty repair here for A's public delta review;
no new wait audit, source-role overreach or full-application run is authorized.

RAW_WRAPPER_INTERPRETATION.json documents inherited r4 wrapper wording/clean
terminal-list fields in the diagnostic receipt. They are preserved as raw, not
used to decide the R5 outcome: direct SIGABRT and the captured failed native
state are authoritative. The actual selector is dirty, with RO OFF.
