# C2P-Cache simulation contract

This branch is an experiment model for the read-side private-L1 sharing
mechanism in *C2P-Cache: Scalable GPU L1 Cache Sharing via Concurrent
Candidate Pruning*.  It is intentionally independent from the RTL worktree.
The goal is to reproduce the mechanism and its trends, not to claim the
paper's absolute IPC, area, or power values.

## Scope

- Eligible requests are global, non-atomic reads that miss in an L1D.
- A C2P transaction owns the original `mem_fetch` until it either fills the
  requester from a remote peer or forwards that unchanged request through the
  original L1-to-L2 path.  It never changes the normal L1 MSHR or fill path.
- Writes, atomics, instruction fetches, and non-global accesses remain on the
  baseline path.  This is the paper's read-side sharing scope.
- `-c2p_cache_oracle_only 1` collects exact peer-L1 availability without
  changing timing or routing.  It is the baseline reference for redundant-L2
  potential.
- `-c2p_cache_ideal_peer 1` performs exact peer discovery, but retains the
  remote tag/return and port model.  It isolates metadata pruning loss from
  sharing potential.

## Metadata model

The implementation follows the paper's stated organization: a 5,120-row
logical Snapshot Matrix, four query rows per line (one reverse low-10-bit tag
mask and three double-hash Bloom positions), 64 banks, and configurable
physical copies (four by default).  Snapshot columns are one bit per SM.

The manuscript does not specify the concrete `h1`/`h2` functions.  This model
uses two deterministic 64-bit folded hashes with fixed salts.  Hash identity
is therefore a sensitivity parameter, not a claimed reproduction detail.

Snapshot bits are updated on every normal L1 fill and are rebuilt from each L1
tag array. A rebuild clears one selected column, transports its
valid compact tags through the shared Update Queue at 128 B/cycle, then uses
the idle share of the BF engines to OR the encoded positions back into that
column. Miss-side queries always take engine priority. The default starts the
next column as soon as the prior rebuild completes
(`snapshot_rebuild_interval=0`), so all 64 columns are refreshed continuously
rather than allowing insertion-only stale bits to accumulate for an arbitrary
long gap. This intentionally permits incomplete or stale bits during rebuilds
and after evictions; all
candidates receive an exact remote L1 tag probe, and a failed probe falls back
to L2, so metadata cannot return incorrect data.

## Timing and contention model

Defaults directly taken from the paper where stated are 128 BF/tag-mask
engines, two-cycle BF/tag-mask latency, two-cycle Snapshot latency,
seven-cycle remote tag latency, two-cycle remote return latency, 64 banks,
four Snapshot copies, and 128 B/cycle update transport. Query and update
queue capacities, the optional idle gap between column rebuilds, probe timeout,
and topology distance are explicit simulator assumptions because the
manuscript does not state them. They are config options and must be reported
with every result. The primary configuration uses no idle gap, matching the
paper's continuous background-refresh description.

Candidate probes are serialized nearest-first.  Each target L1 has a finite
32-entry remote-probe FIFO: a selected candidate waits there until the target
data port is free, then reserves that port for its tag latency.  The timeout
applies only while that FIFO is full, rather than while a useful request is
already queued at its target.  A remote return waits for the requester's fill
port.  If no candidate exists, candidates all miss, or a full target FIFO
stays unavailable through the timeout, the original transaction proceeds to
the ordinary L2 path.

Oracle availability is sampled when the L1 miss enters C2P.  A real remote
probe happens later and can observe a line filled by another SM in the
intervening cycles; therefore realized remote hits are not required to be a
strict subset of the accept-time oracle counter.  The primary
`c2p_snapshot_{TP,TN,FP,FN}` counters follow the paper's miss-time candidate
generation definition and use this accept-time truth.  Separate
`c2p_snapshot_query_*` counters use exact peer residency when the Snapshot
query completes; they diagnose temporal churn without changing the
paper-comparable classification.

Distance uses stable logical SM-cluster order.  In the paper-table overlay
this is eight groups of eight SMs even though the trace-driven simulator
exposes 64 one-SM endpoints for forward progress.  It is deliberately a
lightweight stand-in for the paper's unspecified far-L1 topology, suitable
for comparing same model variants only.

## Required reported variants

Every core result reports at least:

1. **baseline**: C2P disabled;
2. **oracle**: `oracle_only`, for redundant-L2 potential;
3. **ideal peer-L1**: exact peer candidate discovery;
4. **C2P**: Snapshot Matrix pruning and exact serial probes.

The result bundle must retain configuration, trace provenance, simulator and
model commits, C2P counters, and the oracle/ideal/C2P/baseline comparison.
The primary outcomes are redundant L2 reduction, remote-hit rate, candidates
per query, the miss-time Snapshot TP/TN/FP/FN classification, R1S1 speedup,
and R0S1 overhead.  The accept-time oracle counter is reported separately as
the potential redundant-L2 opportunity; query-time Snapshot counters expose
residency churn while the miss waits in the C2P path.

## Prior-mechanism comparison models

`-c2p_cache_scheme` selects the miss-side sharing model when C2P is enabled:
`0` is C2P, `1` ATA-like, `2` CCD-like, and `3` RING-like.  These are
cycle-level mechanism comparisons based on the paper's stated scope,
throughput, and latency assumptions; they are not a claim to reproduce the
unpublished RTL of the cited prior designs.

- **ATA-like** uses exact aggregated tags within an eight-SM cluster, a
  four-request-per-cluster-per-cycle aggregate-tag limit, seven-cycle tag
  lookup, and fourteen-cycle peer line access.  Aggregate tags are sampled
  when the request is issued; the selected L1 is checked again only when the
  later data-array access occurs.
- **CCD-like** uses one weak-taken two-bit saturating predictor per cluster.
  A taken prediction broadcasts to the eight-SM cluster (one-cycle predictor,
  three-cycle broadcast, seven-cycle tag lookup), then exact tags choose the
  peer.  The counter increments on a broadcast hit and decrements on a miss.
- **RING-like** has chip-wide visibility through exact copied tags, serial
  forward ring traversal at two cycles per hop, a seven-cycle tag lookup, and
  fourteen-cycle data access.  Its injection path is serialized, so misses
  pay traversal latency but do not touch peer L1 data arrays.  Like ATA,
  copied tags are sampled at request issue, while data validity is checked at
  the modeled access time.

`c2p_peer_l1_accesses` distinguishes broad ATA/CCD cluster lookups from C2P
candidate probes and RING's hit-only data-array access.  The four C2P core
variants remain required for every main result; ATA/CCD/RING are added for
the same completed workload only after that core bundle passes.
