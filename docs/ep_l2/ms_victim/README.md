# B source r3 — no native qualification

Current r3 closes a source lifecycle gap: completion during a pinned read of
an already-TVD-role slot must finish TVD_INFLIGHT→TVD_VALID at last response
commit too. See `R3_INTEGRATION_AND_LIFECYCLE.md` and source-bound regression
receipt. `adapter_A44_r3` is checked against A's immutable44c940b7 with an
isolated index; no public source was applied. `test_plan_r3` has21 prepared
native process inputs and ZERO actual executions. Earlier r1/r2 directories
and exact commits are preserved. The next section is the prior r2 checkpoint.

# Historical r2 — no native qualification

Current source extends the frozen clean-first89a75b24 candidate with dirty
pending/inflight forwarding and a reciprocal fixed-capacity quota control.
See `R2_DESIGN_AND_VALIDATION.md`, `adapter_r2_full/A_PUBLIC_HOOKS.patch`,
`test_plan_r2/NATIVE_TEST_PLAN.json` and `RELEASE_CANDIDATE.json`.
All18 finite processes remain NOT_COMPILED_NOT_RUN. Hits and DRAM delta are
UNKNOWN. A public files remain untouched; A integrates, E admits/qualifies.
Do not mix r2 consumers with r1 public hooks. The following text is the
**historical r1 checkpoint**, preserved for review; r2 supersedes its pending
and fixed-control implementation status, without conferring qualification.

# Historical r1 source checkpoint — not a runtime release

Authority: Framework d9263fa2431592b33e900d9b48c6f20029d8b544,
mechanism_study_2026_10_19_v1. New branch
`hrl/ep-l2-ms20261019-b-victim-v1`, exact Core parent
84359d4ba492acb639a2092fa271e9169038bf7d. A interface revision1 is
980072bbbcbd4ad00cdf85650283931e94cad395. Runtime Framework parent is
197741be9be9b8e390a557d639503b416d8e95f0; coordination HEAD is not a binary identity.

This checkpoint contains a port of the historical finite TVD directory and
cache consumer, narrow TEXTUAL public hooks, and actual native-cache test
SOURCE. No compiler, native test, simulator binary or full application ran.
Original174 workspace is below48GiB; E must admit later execution. No feature
has new producer or independent qualification. Assertions/PASS strings in test
source are expected outcomes, never evidence of execution.

## Reused and preserved

Historical module/consumer/tests come from be5dcbe1fa5b9a501e81a77a6c399e6f6530e536.
Its r14 follow-up tests were NOT_COMPILED_NOT_RUN. Earlier c197c14d engineering
tests used a different, withdrawn production parent. Neither is transferred as
qualification to84359d4. Entire historical branches are never merged here.

Current public atomic sector/byte masks and completion labels, pending native
Tag-victim binding, WB quota, physical bank clock, RG1/MQ2 and bypass fixes stay
in the parent. B does NOT edit public gpu-cache/cache-partition/config files,
even in this isolated worktree. A alone applies/reviews
`adapter_r1/A_PUBLIC_HOOKS.patch`, and builds all affected objects consistently.
Do not link a changed cache layout to an old archive and call it production.

## First functional target: CLEAN_ONLY

Clean victims retain the exact paid payload handle in place. Reserve a real
same-bank mandatory landing first; otherwise discard optional retention and
reuse the original resident slot. One sole128-row directory replaces WAD
membership when enabled, including handle-less mandatory WB entries; the old
address set is inactive, not a second128-row table. Optional clean/complete
entries can be reclaimed for mandatory directory/landing credit. Pinned
responses and pending/inflight writes cannot be reclaimed.

The serialized frontend starts a victim query after native Tag miss. Result is
usable only in the next physical cycle, at most one victim query per slice per
cycle; an incarnation ticket caches the selected row across bank/ICNT retries.
Retries inspect that row, not repeated free associative victim lookups. A owns
the final WAD/metadata-port integration; known-row updates and WB completion
work must be accounted, not silently treated as additional free CAM ports.
An actual hit pins the real payload, obtains real P1 bank read service, places
the slot's versions in the actual mem_fetch, enqueues the existing response,
then commits/unpins exactly once. Read serve-in-place does not install a Tag.
Missing sectors reclaim optional visibility before ordinary lower fallback.
Clean write/atomic promotes the same handle after preparing real native Tag/WB
resources; full directory promotion reuses its row without a129th entry.

Host prefill receives only the historical exact-alias retirement delta, keeping
clean/quiescent, full address, sector, generation and no-GPU-service guards.
Default-OFF host/atomic behavior is not replaced by an old branch implementation.
Flush/invalidate refuse to drop mandatory/pinned ownership; optional clean rows
are reclaimed through the real allocator before claiming clean termination.

## Separately labelled dirty draft

`DIRTY_INFLIGHT_ONLY` keeps an actual WB payload after independent packet lower
acceptance, forwards resident versions while WB is in flight, preserves WAD
until set_done, then retains clean or permits reclaim/promotion. Writes/atomics
wait through mandatory WB; partial unknown dirty sectors use WAD-only fallback.
Packet snapshots never point at freed storage and must remain immutable.

This is NOT the full original dirty mechanism: WB_STAGED read-forward still
waits, because concurrent same-handle lower-WB and victim reads need distinct
stable logical bank identities in the non-RG1 parent too. No free snapshot or
role/id alias is used to disguise pending forwarding. Full dirty-pending source
adaptation and completion-while-read-pinned native qualification remain open.
The proposed public hooks reject MQ2/RG1, ideals, P2, S64, local quota and C8
combinations with TVD. Joint RO/diagnostic profiles are not qualified.

## Fixed-partition control and remaining work

Same-budget fixed victim partition is NOT_IMPLEMENTED in this checkpoint.
An arbitrary count cap on dynamic retained handles would not be a real fixed
physical partition. The correct control must reserve existing slots (sacrificing
bypass/other capacity), restrict mandatory allocation consistently, and charge
any resident-to-fixed victim copy using the existing128B pipeline latch and
real bank operations. No fixed baseline is fabricated from current dynamic
code. Until implemented/qualified, a future U/dynamic pair can only support
retention's effect, never superiority over fixed partition.

Next: E legal admission -> A-integrated exact source/archives -> module and
native tests -> clean qualification first. Continue fixed control and full dirty
identity/lifecycle adaptation separately, preserving clean deliverability.
No full application is started by B; D selects inputs and E owns qualification
and the24-start ledger. No permanent waiter is installed.
