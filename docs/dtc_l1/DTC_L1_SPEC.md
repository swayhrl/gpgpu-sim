# Decoupled-Tag L1 — M0 Frozen Specification

Status: **M0 FROZEN FOR IMPLEMENTATION PLANNING**

This document captures the architecture/model decisions agreed for the Accel-Sim reproduction of the thesis Decoupled-Tag Cache (DTC-L1). The target is a faithful mechanism-level performance model, not a gate-for-gate reconstruction of the RTL.

## 1. Source and branch anchors

Core upstream:

- repository: `accel-sim/gpgpu-sim_distribution`
- branch: `dev`
- frozen base SHA: `91880c53383d5a6a6742bfb1be2c5f34e39c7871`

Working branch:

- repository: `swayhrl/gpgpu-sim`
- branch: `hrl/decoupled-l1-v0`

Experiment/coordination branch:

- repository: `swayhrl/accel-sim-framework`
- branch: `hrl/decoupled-l1-exp-v0`

## 2. Modeling objective

The simulator must preserve the causal mechanisms that matter to the paper results:

1. bounded pending-instruction capacity and backpressure;
2. logical Tag to physical Cacheline renaming;
3. physical-line allocation pressure with partial allocation and no rollback;
4. removal of the traditional DTC-side MSHR capacity bottleneck;
5. pending-hit merging without issuing duplicate lower requests while the logical tag remains present;
6. IO head-of-line behavior and release semantics;
7. OO completion, merge wakeup, reference tracking, and active reclamation;
8. finite Tag-bank throughput, finite physical allocation width, finite retire width, and finite lower-request injection/outstanding capacity.

Do not make internal operations unbounded or zero-cost merely because the simulator could do so.

## 3. Paper-mode default geometry

All values below are configurable defaults.

### 3.1 Warp/front end

- warp width: **32 threads**;
- simulator default coalescer processing width: **32 threads/cycle**;
- original RTL area-constrained implementation processed **16 threads/cycle**; preserve `16` as a sensitivity/configuration option;
- a fully divergent 32-thread warp can therefore generate up to **32 distinct 128B cacheline references**.

The simulator is allowed to process all 32 threads in one cycle because Base/IO/OO will use the same front-end abstraction. The 16-thread setting remains available for paper/RTL sensitivity.

### 3.2 Logical Tag Array

- logical capacity: **16KB**;
- line size: **128B**;
- logical lines: **128**;
- associativity: **4-way**;
- sets: **32**;
- replacement: **LRU**;
- Tag partitions/banks: **4**;
- sets per Tag bank: **8**;
- entries per Tag bank: **8 sets × 4 ways = 32 entries**;
- bank mapping: `tag_bank = logical_set_index % 4`;
- bank throughput: **1 Tag request/bank/cycle**, at most **4 Tag requests/cycle** globally;
- same-bank requests are arbitrated and remaining requests wait/backpressure according to the pipeline/buffer state.

Tag-bank behavior must be explicit in the model.

A logical Tag entry conceptually contains at least:

- valid state;
- address tag;
- physical cacheline identifier/index;
- replacement/LRU metadata.

OO may require additional Tag/physical-lifetime metadata as specified below.

### 3.3 Physical Cacheline Array

- physical capacity: **80KB**;
- line size: **128B**;
- physical lines: **640**;
- the full 80KB belongs to L1; it is **not partitioned with shared memory** in the paper-mode model.

The logical Tag location does **not** constrain the physical-line location. Do not bind Tag bank N to a corresponding Data bank N.

### 3.4 Physical allocation model

Frozen mechanism-level abstraction:

- allocator policy: **round-robin** over free physical lines;
- maximum successful allocations: **4 physical lines/cycle**;
- partial allocation across a multi-line memory instruction is allowed;
- allocations already made for an instruction are retained if later lines cannot be allocated;
- there is **no rollback** of those partial allocations;
- a released physical line is visible to allocation in the **same cycle**;
- if the current instruction cannot obtain all required lines, it remains partially allocated/stalled and retains the lines it already obtained.

The original RTL motivation is four allocation banks with one allocation/bank/cycle. The simulator may abstract this as a global RR free-line pool with `alloc_width=4` because the first reproduction does not require an explicit Data-bank mapping/conflict model.

Important: do **not** add an atomic all-or-nothing allocation policy. The partial-allocation behavior is necessary to allow the IO-DTC circular resource dependency/deadlock to emerge when physical space is too small.

Do not encode a capacity-specific deadlock special case; deadlock must emerge naturally from held partial allocations, FIFO retirement constraints, and lack of free physical lines.

### 3.5 Data-bank abstraction

Default paper-mode model:

- no explicit Data-bank mapping is required;
- no Data-bank conflict penalty is modeled initially;
- physical allocation width remains finite at 4 lines/cycle;
- retire width remains finite at one instruction/cycle.

Keep configuration hooks for a later optional model such as:

- `physical_banks = 4`;
- `model_data_bank_conflict = false` by default;
- if enabled later, a simple candidate mapping is `data_bank = phys_id % physical_banks` unless a better source-backed mapping is established.

## 4. Pipeline and queue semantics

### 4.1 Pipeline latency

For M0/M1 planning, every pipeline stage shown as one stage/box in the reference architecture is modeled as **one simulator cycle**.

We do not need gate-level or sub-stage fidelity unless a later sensitivity study shows a material performance dependence.

### 4.2 Backpressure

All bounded queues/buffers use normal backpressure:

- when a stage's destination FIFO/buffer is full, that stage stalls;
- the stall propagates backward through upstream stages;
- ultimately the memory-instruction entrance stalls when the pending-instruction structure cannot accept more work.

No request may be silently dropped or bypass a full structure unless architectural bypass semantics explicitly require it.

## 5. Pending Instruction Buffer (PIB)

All depths are configurable.

Paper defaults:

- Baseline PIB: **8 entries**;
- IO-DTC PIB/FIFO: **256 entries**;
- OO-DTC PIB/random-access buffer: **128 entries**.

IO and OO defaults intentionally differ; do not hard-code capacity into the mode implementation. The same implementation must support sweeps such as 32/64/128/192/256 entries.

### 5.1 Retire width

- IO retire width: **1 memory instruction/cycle**;
- OO retire width: **1 memory instruction/cycle**.

OO may choose a ready younger entry, but it may not retire an unbounded number of ready entries in one cycle.

## 6. Lower-memory request limits

Paper-mode default GPU configuration:

- SM count: **8**;
- each SM's L1 may inject at most **1 request/cycle** toward NoC/L2;
- global lower-memory outstanding-request limit: **256 requests across the 8 SMs**;
- reaching the global limit is treated as the Miss-Queue/lower-request-capacity full condition for this mechanism model;
- all values must be configurable.

This is a deliberate mechanism-level abstraction. If the current official source exposes a more natural equivalent queue structure, Codex must document it before changing the semantics.

## 7. Baseline model

Paper-reproduction baseline defaults:

- L1 capacity: **16KB**;
- line size: **128B**;
- associativity: **4-way**;
- replacement: **LRU**;
- explicit thesis-style PIB capacity: **8 entries**;
- traditional MSHR capacity: **32 entries**.

The baseline PIB is modeled as a bounded pending-instruction structure at approximately the same front-end point as DTC PIB admission. Exact source insertion point is an implementation question, but a full PIB must eventually backpressure the memory-instruction entrance.

Instrumentation must distinguish at least the dominant primary reasons:

- PIB full;
- Tag/cacheline allocation failure;
- MSHR entry/merge full as applicable;
- miss/lower-request queue full;
- Tag-bank conflict;
- no free physical line for DTC.

When thesis-style PIB gating/instrumentation is disabled, the modified baseline must remain timing neutral relative to the selected clean baseline.

## 8. IO-DTC semantics

IO uses an in-order FIFO PIB.

Core behavior:

1. coalesced line requests pass through Tag-bank arbitration;
2. hits record the referenced physical line;
3. misses choose a logical Tag victim by 4-way LRU and allocate a free physical line;
4. the new logical Tag→physical mapping is installed without a traditional Reserved-tag/MSHR allocation protocol;
5. the old physical line displaced by a Tag replacement is retained as a release dependency rather than immediately reused;
6. lower requests are issued subject to the finite issue/outstanding limits;
7. the FIFO head may retire only when all required data are ready;
8. retirement releases the appropriate old physical-line dependencies;
9. release is visible to allocation in the same cycle.

IO does not require OO-style Ref Count or random ready selection because strict FIFO order protects older physical-line users.

Physical-space deadlock is a valid emergent behavior in undersized configurations. The critical mechanism is that a multi-line request may hold some newly allocated physical lines while waiting for additional lines, while FIFO ordering prevents the releases needed to make progress.

## 9. OO-DTC semantics

OO uses a random-access PIB with one-entry-per-dynamic-memory-instruction occupancy and `retire_width=1`.

Physical-line metadata conceptually includes:

- logical-Tag visibility (`tag_valid` or equivalent);
- physical data readiness state;
- reference count;
- merge/wakeup information for pending data.

A physical line is reclaimable only when:

`tag_valid == 0 && ref_count == 0`

A logical Tag replacement clears the old physical line's `tag_valid` but does not free it while references remain.

A ready PIB entry may retire out of program/FIFO order subject to the one-instruction-per-cycle retire bandwidth and any required architectural ordering constraints added in later stages.

## 10. Ref Count — frozen granularity

The reproduction uses **per-coalesced-cacheline-reference** Ref Count semantics.

Definition:

> one coalesced **128B cacheline reference/request** held by a live PIB instruction contributes one reference to the referenced physical 128B line.

Consequences:

- multiple lanes coalesced into one 128B cacheline request contribute **+1**, not +N lanes;
- a fully divergent 32-thread warp can generate at most 32 distinct 128B cacheline references;
- OO PIB depth 128 gives the conservative upper bound `128 × 32 = 4096`, so the paper-compatible default Ref Count width is **13 bits**;
- Ref Count is a **physical-line-level** lifetime counter, not a sector-level counter;
- the corresponding reference is decremented when that PIB instruction releases/completes that coalesced line dependency according to the OO retirement semantics.

Do not reinterpret the counter as per-thread without a later explicit specification change.

## 11. Pending-hit merging and lower-request identity

DTC must not rely on the traditional MSHR table for its own capacity/merge semantics.

While a logical tag maps to a physical line/sector already pending:

- a later coalesced request attaches to that pending physical allocation;
- it must not issue a duplicate lower request for the same pending data;
- completion wakes every attached dependency exactly once.

A Tag can later be evicted while an old physical allocation/request is still in flight. Therefore a lower-memory response must be associated with the intended **physical allocation identity**, not merely by re-looking up the current logical Tag address.

Implementation may use an existing request UID or an explicit `{phys_id, generation}`-style identity. Exact representation is an M1 implementation decision, but stale fills into a recycled physical line are forbidden.

## 12. Whole-line paper mode

The primary reproduction of the thesis figures uses a **whole-line 128B DTC mode**:

- Tag→Physical renaming granularity: 128B;
- data INVALID/PENDING/VALID state granularity: 128B;
- fill/miss dependency granularity: 128B;
- Merge Mask/wakeup granularity: 128B.

This mode is the reference for paper-reproduction experiments and should be implemented/validated before the modern sector extension is treated as formal evidence.

## 13. Modern sector extension

The modern Accel-Sim extension preserves the original renaming granularity while refining data readiness:

- Tag→Physical mapping remains **one 128B logical line → one 128B physical line**;
- physical line is divided into **4 × 32B sectors** for readiness;
- each sector has INVALID/PENDING/VALID state;
- OO Merge Mask/wakeup state is per sector;
- `wait_cnt` counts outstanding **sector dependencies**;
- Ref Count remains **line-level per coalesced 128B cacheline reference**, not per sector;
- one coalesced 128B line reference may carry a sector mask and still contributes only one line-level Ref Count increment;
- a physical line can be reclaimed only when line-level `tag_valid == 0` and line-level `ref_count == 0`.

Example:

- P10/S0 valid, S1 pending, S2 invalid;
- one instruction references S0+S1+S2 within P10;
- line-level Ref Count contribution: +1 for P10;
- `wait_cnt`: +2 (S1 and S2 not ready);
- S1/S2 fill independently decrement sector dependencies;
- retirement decrements the single line-level P10 reference.

The sector model is an extension and its results must be reported separately from whole-line paper reproduction when the distinction matters.

## 14. Store, Atomic, Fence, and bypass staging

These are not required to prove the read-path DTC mechanism in the first implementation stage, but they matter for complete workloads and the final IO-vs-OO comparison.

Frozen staging boundary:

- Load/read DTC path: first implementation priority;
- Store: may be temporarily routed through the existing baseline path for bring-up only; final compute experiments require it to participate in the DTC instruction lifecycle as specified later;
- Atomic: deferred until the compute-complete stage; reuse existing lower-level atomic semantics rather than reinventing the memory system;
- Fence/ordering: deferred until the compute-complete stage;
- existing architectural L1 bypass semantics must remain intact from the beginning;
- DTC policy-driven bypass from the thesis is a later extension, not part of the first read-path implementation.

Do not use temporary Store/Atomic bypass bring-up results as formal IO-vs-OO performance evidence.

## 15. Parameterization checklist

Names may follow existing code conventions, but equivalent knobs must exist where practical:

- mode: `baseline | io | oo`;
- logical capacity KB;
- physical capacity KB;
- line size;
- associativity;
- replacement policy;
- Tag bank count;
- Tag requests per bank per cycle;
- coalescer threads per cycle;
- physical allocation width;
- physical allocator policy;
- optional physical/Data bank count and conflict modeling enable;
- PIB entries;
- retire width;
- per-SM lower-request issue width;
- global outstanding lower-request limit;
- sector enable/count/size;
- Ref Count width;
- assertions/debug stats enable;
- deadlock/no-progress watchdog parameters.

Preset defaults may encode the paper configuration, but behavior must not depend on magic constants.

## 16. Required observability for later stages

The implementation must eventually expose enough information to explain—not merely measure—performance:

- PIB occupancy average/peak/distribution;
- primary stall reason and non-exclusive resource-unavailable counters;
- Tag-bank conflict cycles;
- physical-line allocated/free occupancy;
- partial-allocation stalls;
- IO head-block cycles and ready-younger count;
- lower-memory outstanding count over time;
- Valid/Pending/NewMiss counts;
- merge fanout;
- Ref Count distribution and increment/decrement balance;
- physical-line lifetime/pending duration;
- duplicate lower requests after logical-Tag eviction if they occur;
- L1→L2 and L2→memory traffic/bandwidth;
- memory-instruction latency distribution.

Exact counter names belong to M1-M3 planning.

## 17. M0 unresolved implementation questions

These are not architecture ambiguity; they are source-integration questions to be resolved during implementation planning/audit:

1. exact clean insertion point for the explicit Baseline/DTC PIB in the current upstream code;
2. exact representation for physical allocation identity on fill/return;
3. how current Accel-Sim sector-cache classes should be reused vs wrapped for the whole-line paper mode;
4. source-of-truth relationship between the standalone core repository and the framework's `gpu-simulator` tree;
5. exact Store/Atomic/Fence lifecycle integration for the compute-complete stage;
6. exact statistics plumbing and run-script/config preset placement.

Do not guess these by changing architecture semantics. Surface them in the Codex handoff/review pack.

## 18. M0 acceptance statement

M0 is considered frozen when:

- the geometry, capacities, bank mapping, allocation width/policy, PIB depths, retire width, lower-request limits, pipeline/backpressure abstraction, IO/OO release semantics, Ref Count granularity, and sector-extension rules above are treated as authoritative defaults;
- all sensitive values remain configurable;
- remaining uncertainties are implementation-placement questions rather than unrecorded architecture guesses.

No implementation stage should silently alter this document's semantics. Any proposed change must be reported and reviewed before becoming formal experimental configuration.
