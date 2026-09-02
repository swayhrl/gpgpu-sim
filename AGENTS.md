# AGENTS.md — GPGPU-Sim Core Guardrails for VM/TLB Research

This branch is the Core half of the VM/TLB research project coordinated from `swayhrl/accel-sim-framework` branch `hrl/vm-core-v0`.

Frozen Core base:
`73774727e25fadf89df6f30ef5cf014091115db7`

Framework coordination state lives in:
`docs/vm_tlb/` of `swayhrl/accel-sim-framework@hrl/vm-core-v0`.

Before any Core modification, read the Framework project's:

1. `docs/vm_tlb/chatgpt_handoff/CURRENT_STATE.md`
2. `docs/vm_tlb/chatgpt_handoff/DISCUSSION_REFERENCE.md`
3. `docs/vm_tlb/chatgpt_handoff/CODEX_NEXT_STAGE.md`
4. root `AGENTS.md`

If those files do not explicitly authorize Core changes for the current stage, do not modify Core source.

## Core semantic guardrails

- Treat trace addresses as simulator `SimVA` by modeling contract; do not present that as proof of the exact internal NVIDIA address stage captured by NVBit.
- Translation produces `SimPA`; preserve both VA and PA identities for observability.
- Initial functional mapping is identity-like (`SimPPN = SimVPN`) so data-cache/DRAM locality is unchanged.
- Translation is expected on coalesced memory transactions before real L1D/data-cache access unless an approved stage specification says otherwise.
- M1–M3 are resident-memory translation work: no page fault, migration, UVM oversubscription, or CPU fault service unless explicitly authorized.
- Default modeling decision is that TLB state persists across ordinary kernels in the same simulated context.
- PTE memory requests are physical and must never recursively enter normal address translation.
- At most one active page walk may exist for one `(ASID, VPN, page-size-class)`; later misses merge or backpressure according to the approved translation-MSHR policy.
- Translation-MSHR merge is not a TLB hit.
- Each waiter is registered once and awakened once.
- No data-cache access before translation completion.
- No duplicate store/atomic due to translation replay.
- Enforce queue/MSHR/walker capacity as behavior, not statistics only.
- `active_walkers <= configured_walkers` at all times.
- VM-disabled behavior must remain free of added VM latency/queues/mapping effects.

Any failure of these invariants blocks performance experiments.

## Reference code policy

The old `dev-uvm` implementation and TLS/MCM code may be inspected for interface ideas. They are not authoritative semantics and must not be wholesale cherry-picked unless the current Framework handoff explicitly authorizes it.

Prefer minimal, reviewable changes. Do not mix large cleanup/refactoring with new VM behavior in one commit unless unavoidable and documented.

## Evidence labels

Use the Framework project labels consistently:

- `VERIFIED_CODE`
- `VERIFIED_RUN`
- `PAPER_SPEC`
- `USER_CONFIRMED`
- `MODELING_DECISION`
- `HYPOTHESIS`
- `UNKNOWN`
- `PAPER_EXACT`
- `DOCUMENTED_APPROX`

Do not silently turn an approximation into a paper-exact implementation.

## Validation

Every semantic Core stage requires directed tests before workload characterization. Tests must verify expected counts/invariants, not just exit successfully.

At closeout record exact Core SHA, Framework SHA, config/trace identity, test commands, wall-clock, and result status.

## Git/worktree rules

Do not push to the official upstream Accel-Sim/GPGPU-Sim repository.

The writable project repository is `swayhrl/gpgpu-sim` and the project branch is `hrl/vm-core-v0` unless a later handoff creates a stage branch.

Forbidden:

- `git add .`
- `git add -A`
- unapproved force-push
- modifying a frozen worktree used by another run

Stage explicit paths only and keep commits semantic.

Before closeout run relevant tests plus `git diff --check` and `git status --short`.

## STOP conditions

Stop rather than improvising when:

- a correctness invariant fails;
- baseline/identity transparency fails;
- a paper detail materially affecting implementation is unknown;
- a required `PAPER_EXACT` detail would need an unapproved approximation;
- deadlock, lost request, duplicate wakeup, duplicate side effect, or unexplained nondeterminism appears;
- the next macro task is not explicitly authorized by the Framework handoff.

Do not proceed into performance characterization with unresolved correctness.
