# R3 — A44 integration adapter and source lifecycle correction

SOURCE_ONLY_NOT_COMPILED_NOT_RUN. Full objective remains incomplete: no actual
cache hit/returned-version/native DRAM change has yet been demonstrated in this
campaign. Clean-first89a75b24 and pending/fixed r2 b90ed7c1 remain frozen.

## Specific source defect and correction

Consider a retained dirty WB that was already accepted before a later victim
read pinned its slot: directory WB_INFLIGHT, payload roleTVD/TVD_INFLIGHT.
If ICNT space becomes unavailable while its bank retry is pending/selected,
set_done advances the directory to WB_COMPLETE but defers slot state while
pinned. R2 response_commit only finishes that transition when role==WB. This
already-TVD slot would retain TVD_INFLIGHT after its final unpin. That is a
source state-machine defect; its downstream effects have NOT been measured.

R3 last-unpin checks the directory's WB_INFLIGHT/WB_COMPLETE state for BOTH
WB and TVD roles, asserts allowed ownership and sets the exact target role/
state. No payload/generation migration, free response buffer or premature
unpin is introduced. It occurs only after paid bank service and actual ICNT
enqueue. The older pending-WB-role deferred transition remains supported.

`REGRESSION_SOURCE_BINDING_r3.json` binds the old consumer bytes, corrected
consumer and new test. Old-source failure is expected, NOT observed. A/E can
compare the same A44/adapters/test source varying only old/new consumer under
legal native admission; no such compile or run has occurred.

## Two additional real-path test sources

`dirty_icnt_pinned`: actual primary1024Tag/1168/D512 cache traffic retains a
dirty victim after lower acceptance. Genuine63 returned reads fill63 of the
existing64 response FIFO entries. Another real ReturnQ fill uses a different
physical bank and makes the final response ready; an explicit public-arbiter
contender denies the victim read before it consumes bank service. That next
native response fills the FIFO before victim retry. The selected victim token,
pin, generation and versions must survive four blocked cycles and real WB
completion. After FIFO consumption, one real response retires the pin and
publishes TVD_VALID, with no duplicate lower read or pending token. No direct
FIFO insertion/state mutation or naturally occurring application claim.

`clean_wb_reuse`: CLEAN_ONLY accepts WB's independent packet and releases its
local slot while preserving mandatory handle-less WAD. An actual unrelated
native fill reuses that exact slot with a new generation. A same-address read
must wait, with no duplicate lower issue, until old set_done. Completion must
leave the new owner/versions intact and old packet snapshot immutable; then
the original address uses one real lower read. No fixture frees/mutates the
cache slot or fabricates a response.

The additional `--study-dirty-boundaries` selector reuses existing bank-local
pool exhaustion/reclaim and partial-dirty promotion cases. Their unchanged
assertions have not run here;1024x1 versus64x16 geometry is labelled in source.

All21 process entries in test_plan_r3 are NOT_COMPILED_NOT_RUN. Configs are
prepared inputs, not an effective runtime dump. External memory validates
nonzero sector VERSION labels and independent lower-packet counts; native
simple-DRAM fixtures separately validate actual native DRAM issue counts.
Numerical partial-byte/atomic arithmetic, app timing, RTL/PPA remain outside
this test evidence. Existing negative/zero results are retained.

## Exact A44 textual integration

`adapter_A44_r3/A_PUBLIC_HOOKS.patch` is generated against immutable A Core
44c940b748fceaec8f21fa2b443907d5afd2756b; no A working path or B public source
is modified. The patch checks against a separate index made from those exact
Git blobs. All six actual B public files remain identical to84359d4 and the
real index is unchanged by this check. No native compiler/parser has run.

A's COPY_FREE/DATA_FREE code and observer functions remain in place. A's old
combination guard cannot know new TVD flags, so a new explicit guard rejects
TVD plus local diagnostic OR observer until those paths are independently
integrated/qualified. TVD-OFF independent A diagnostic/observer profiles are
preserved in source; runtime equivalence is not claimed. Existing r1/r2 patches
are not regenerated or overwritten. Do not mix consumers/adapters across
versions, cherry-pick whole historical branches or replace A's public worktree.

A still must review shared WAD/TVD metadata query/update/completion ports,
normal-mode OFF equivalence, reciprocal fixed64 capacity/control cost, real
rollback boundaries and the final combined exact source/ABI. E alone admits
scientific execution and independently qualifies. No E admission was available
at this continuation's initial check; no repeated disk survey, native test,
full application, lease or watcher is started by B.
