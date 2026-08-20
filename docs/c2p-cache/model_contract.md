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

Snapshot bits are updated on every normal L1 fill and are periodically rebuilt
from each L1 tag array.  This intentionally permits stale bits between
rebuilds; all candidates receive an exact remote L1 tag probe, and a failed
probe falls back to L2, so stale metadata cannot return incorrect data.

## Timing and contention model

Defaults directly taken from the paper where stated are 128 BF/tag-mask
engines, two-cycle BF/tag-mask latency, two-cycle Snapshot latency,
seven-cycle remote tag latency, two-cycle remote return latency, 64 banks, and
four Snapshot copies.  Query and update queues, background rebuild interval,
probe timeout, and topology distance are explicit simulator assumptions because
the manuscript does not state them.  They are config options and must be
reported with every result.

Candidate probes are serialized nearest-first.  A remote probe reserves the
target L1 data port for its tag latency; a remote return waits for the
requester's fill port.  If no candidate exists, candidates all miss, or a
target stays busy through the timeout, the original transaction proceeds to
the ordinary L2 path.

Oracle availability is sampled when the L1 miss enters C2P.  A real remote
probe happens later and can observe a line filled by another SM in the
intervening cycles; therefore realized remote hits are not required to be a
strict subset of the accept-time oracle counter.

Distance uses stable SM/cluster order.  It is deliberately a lightweight
stand-in for the paper's unspecified far-L1 topology, suitable for comparing
same model variants only.

## Required reported variants

Every core result reports at least:

1. **baseline**: C2P disabled;
2. **oracle**: `oracle_only`, for redundant-L2 potential;
3. **ideal peer-L1**: exact peer candidate discovery;
4. **C2P**: Snapshot Matrix pruning and exact serial probes.

The result bundle must retain configuration, trace provenance, simulator and
model commits, C2P counters, and the oracle/ideal/C2P/baseline comparison.
The primary outcomes are redundant L2 reduction, remote-hit rate, candidates
per query, R1S1 speedup, and R0S1 overhead.  ATA, CCD, RING, Pannotia/ISPASS,
and PPA are intentionally outside this phase.
