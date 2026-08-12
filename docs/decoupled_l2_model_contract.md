# Decoupled L2 experiment model contract

## Purpose

`decoupled_l2` is an experimental C++ L2 backend for GPGPU-Sim.  It models
the scheduling consequences of the V3 RTL architecture, not its packed XBUS
interface or its bit-level implementation.  It is selected per run and leaves
the existing `l2_cache` backend unchanged.

The model keeps the parts that matter to memory-system experiments:

- a token-owned request table;
- a line-keyed active-address directory (AAD), including same-line chaining;
- an outstanding-fill (OTF) record per line;
- decoupled tag, lower-read, lower-write, response, and bank work queues; and
- finite queue and bank resources that generate backpressure.

## Deliberate scope

The model consumes the existing GPGPU-Sim `mem_fetch` interface and cache-line
addressing.  It supports ordinary global reads and writes, writebacks, and the
simulator's atomic request class.  It does **not** implement the RTL XBUS,
CREQ, IREQ, VREQ, AFREQ, ECC, data/ECC arrays, or byte-accurate atomic
execution.  Functional atomic effects remain where GPGPU-Sim already applies
them, when a completed `mem_fetch` leaves the memory sub-partition.

The model is timing/ordering research infrastructure.  Its metadata must stay
internally consistent, but it is not a substitute for RTL verification.

## Backend contract

At every memory sub-partition, the selected backend owns an accepted request
until exactly one of these happens:

1. it returns the original `mem_fetch` through the L2-to-interconnect queue;
2. it emits a lower-memory request and later accepts the matching fill; or
3. it consumes an internal writeback acknowledgement.

An accepted request is never re-presented to the front-end queue.  A lower
read owns one line OTF record; later requests to that line attach to the same
AAD chain rather than issue another lower read.  AAD entries are released only
after all chain members have been completed or converted to a lower-write
operation.  Assertions enforce those uniqueness rules in debug builds.

## Time model

All phases advance in `memory_sub_partition::cache_cycle()` clock units:

```
front-end admission -> tag phase -> {hit response | OTF lower read}
lower read fill    -> bank/data phase -> response phase
write/eviction     -> WBQ lower write -> acknowledgement
```

The initial selectable `fixed` mode uses only the response phase and is a
sanity backend: it returns every accepted request after a fixed latency,
without allocating tags or issuing DRAM traffic.  The full `decoupled` mode
adds the later phases incrementally.  Each configuration parameter is in L2
cycles and is intentionally independent of baseline-cache timing knobs.

## Correctness invariants

- A request token is either free or appears in exactly one live phase/chain.
- At most one AAD head and one OTF record exist for a line in a sub-partition.
- Every OTF record has exactly one lower read in flight until its fill arrives.
- A dirty victim enters WBQ exactly once and is not reusable before its write
  acknowledgement.
- Lower reads in flight are credit-limited per sub-partition.  Independently,
  the decoupled L2 leaves one L2-to-DRAM FIFO entry available for a WBQ
  writeback.  Thus a full WBQ cannot block fills while its own writeback is
  unable to enter DRAM behind lower reads.
- A bank accepts at most one operation per modelled bank cycle.
- No completion is dropped when the L2-to-interconnect queue is full.

These are model assertions, not replacement RTL assertions.  Counters expose
admissions, merges, OTFs, writebacks, stalls, and per-bank utilization so a
run can be rejected when an invariant trips.

## Configuration and reproducibility

`-gpgpu_l2_backend baseline` is the default and preserves current behaviour.
`-gpgpu_l2_backend fixed` and `-gpgpu_l2_backend decoupled` are opt-in.  The
Accel-Sim experiment wrapper must source the selected GPGPU-Sim worktree first
and reject a conflicting `GPGPUSIM_ROOT`; this avoids accidentally linking an
unrelated nested checkout.

Every reported run records the GPGPU-Sim commit, Accel-Sim commit, backend
mode, and all `-gpgpu_decoupled_l2_*` parameters.

`-gpgpu_decoupled_l2_lower_read_entries` defaults to 32.  It bounds returned
lower reads per subpartition; the independent L2-to-DRAM FIFO reservation keeps
one writeback injection slot regardless of that setting.
