# C2P power-activity interface

## Purpose and boundary

AccelWattch already consumes aggregate activity for the baseline GPU's L1,
L2, NoC, DRAM, and cores.  C2P's model is currently a C++ miss-side model, so
those baseline counters do not account for the C2P-specific structures.  This
interface defines the additional event producer before attaching it to an
AccelWattch component model.

The interface is an activity contract, not an energy claim.  Energy per event
and static/leakage power are supplied later from CACTI/SRAM characterization
and a synthesized control implementation.

## Producer

`c2p_cache` owns all C2P event counters.  A counter increments at the
architectural event, not when a transaction merely enters a software state:

| Event | Counter | Increment point |
| --- | --- | --- |
| BF query encode | `bf_query_encodes` | a miss consumes one of the 128 BF engines |
| BF update encode | `bf_update_encodes` | an update consumes a BF engine |
| Snapshot row read | `snapshot_row_reads` | a query's four rows are scheduled/read |
| Snapshot row write | `snapshot_row_writes` | an encoded fill/rebuild tag ORs its four positions |
| Snapshot-column clear | `snapshot_column_clears` | a periodic rebuild clears a column |
| Remote request enqueue | `remote_request_enqueues` | a selected C2P candidate enters its target FIFO |
| Target FIFO dequeue | `remote_request_dequeues` | a target port accepts the FIFO head |
| Remote tag access | `remote_tag_accesses` | an exact target-L1 probe starts |
| Remote response | `remote_responses` | an exact remote hit enters return latency |
| Target-FIFO timeout | `remote_probe_timeouts` | a queued request is removed for L2 fallback |

The existing `peer_probes`, `remote_hits`, `snapshot_updates`, and
`snapshot_rebuild_transport_tags` remain performance statistics.  They are
not substituted for the event counters above because they do not distinguish
enqueue/dequeue timing or query/update BF usage.

## Consumer

A `c2p_power_activity` bridge will snapshot the producer counters at each
AccelWattch sampling interval and pass deltas to four new power components:

1. `C2P_SNAPSHOT`: row reads/writes, column clears, and SRAM leakage;
2. `C2P_BF`: query/update BF encodes;
3. `C2P_REMOTE_CTRL`: FIFO enqueue/dequeue and timeout bookkeeping;
4. `C2P_REMOTE_NET`: request/response flits or explicit fixed-width messages.

The bridge must not add C2P traffic to the baseline simulator NoC counter:
the model uses a dedicated far-L1 transport, while baseline NoC activity is
already reported by AccelWattch.  If a future model routes C2P over the normal
interconnect, this rule must be revised to avoid double counting.

## Calibration inputs and reporting

- Snapshot SRAM read/write energy, leakage, and area come from the retained
  CACTI 40 KiB / 64-bank / 8-byte-row single-copy proxy.  Four copies are
  reported as separately scaled SRAM banks.
- BF and control energy/area come from a future C2P RTL synthesis result.
- Remote-network energy uses an explicitly recorded flit width and hop model;
  no value may be silently inferred from `remote_hits`.

Until all four components have calibrated energy coefficients, an
AccelWattch report is labeled **baseline-component-only**.  It may compare
changed L1/L2/DRAM/normal-NoC activity between baseline and C2P, but it is not
reported as C2P total chip power or power-per-watt.
