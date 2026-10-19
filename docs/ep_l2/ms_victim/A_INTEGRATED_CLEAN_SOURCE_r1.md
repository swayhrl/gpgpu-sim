# A integrated CLEAN_ONLY source candidate — NOT_COMPILED_NOT_RUN

A parent17e7e224954e44a968a0447bcdfea91e3b2e5a96; frozen B module input
89a75b2444e21229357631248f4e2043fd849ad0; B control receipt ebfb5371086241d90865f61b49baa54989c876a5.
The five public adapter files were reconstructed on exact84359d4 in a private
index (tree91b68d0aaa537db600e4853364a7fa3c8f6b0f53), then individually merged
with A. Four textual conflict sites resolved once. No whole historical branch
merge. Old B README/manifests/plan remain provenance of B input, not current
A source qualification. This document and the A Framework receipt supersede
those files for integrated status. R1 build source/worktree remains separate.

Only normal Unified/P1 CLEAN_ONLY is enabled as a source candidate. Dirty
pending/inflight, fixed partition, RO, diagnostics/observer + TVD are rejected
or unimplemented. Existing S/U/local-diagnostic modes remain in the binary
source with TVD defaultOFF; their fresh affected validation is still required.
No code in this worktree was compiled, executed, or linked to old archives.

## Actual common adaptations and model cost

- Shared128 directory entries replace functional old WAD set while enabled;
  mandatory WB rows and optional clean rows share that128. Real1168 payloads,
  resident1024 Tags, D512, bank=id%4 and four P1 banks are unchanged. Host
  timestamps/counter maps are observation-only, not a second functional index.
- Frontend Tag-miss waits one physical cycle and performs at most one victim
  CAM query/slice/cycle. The row-incarnation ticket is cached through retries;
  normal WAD predicates reuse that result. An unqueried MISS cannot enter
  ft_access. A resident hit cannot alias a mandatory WB by lifecycle invariant.
- Lower WB acceptance and rollback use the row ticket in the existing bounded
  WB record / one serialized replacement context, not another address CAM.
- Completion uses ONE separately declared address-CAM path per slice/cycle,
  then checks the packet's EXISTING request UID against32-bit UID+bound flag
  stored in that directory row BEFORE mutation. The current source asserts the
  single completion-port rate; an excess is a failed candidate, not free extra
  service. No packet field, flit count, NoC/DRAM model, or wire identity is added.
  Original32-bit request-UID wrap limits remain; payload GEN32 and directory
  incarnation64 are unchanged. Lower acceptance still releases WB-only data;
  completion owns mandatory row release, and old row tickets remain stale.
- Native resident eviction/promotion proves owner/handle disjointness from the
  directory. Duplicate full scans become explicit debug invariant assertions,
  not hidden functional CAM ports. Generic unproven module callers retain the
  full validation. Invalid native ownership aborts; it does not silently bypass
  the invariant. Before acceptance, real cases must validate these premises.
- Cost ledger: TWO declared CAM read paths (frontend victim1, WB completion1),
  up to THREE indexed control read views and THREE update event lanes
  (frontend, lower-accept, completion) on metadata FF/control. Free/age/bank
  selection are additional combinational encoders; no single-port SRAM model
  is claimed for this metadata. Directory adds WB-UID32+bound1 per row, plus
  existing row incarnation/owner/valid/dirty/reader/age fields. Existing WB
  local records and replacement context each gain row32+incarnation64 until
  lower acceptance/rollback. These are explicit control costs, NOT another
  directory/request pool. Their timing/area is UNKNOWN; old R3 PPA cannot be
  reused. Actual metadata-rate/ordering qualification and scientific review
  of these declared control paths are REQUIRED before application acceptance.
- Normal controller bypass alone now waits for a full lower FIFO. No global
  lower-full condition is added to ordinary cache hits. Native regression
  source holds one real bypass FIFO entry and checks the next head survives.

## Finite pending evidence and supported boundary

Current targets ep_l2_ms_victim_directory / _native / _production are opt-in.
Prepared clean set: directory--clean; OFF/ON cycle17; OFF/ON stream17; clean
sector3/1/0/2; missing-sector lower fallback; real CLEAN_ONLY WB snapshot/UID/
mandatory-wait/row-reuse; texture-only bypass FIFO full; native simple-DRAM
--study-clean; native --r14-integration-gates. Eleven processes, all NOT_RUN.
The external-memory fixture serializes completions to the declared one-port
rate; it remains distinct from actual simple-DRAM timing/issue evidence.

The shared first diagnostic55 affected cases must also be qualified on the
new ABI before any R2 runtime release; none is inherited as a PASS. Atomic ALU
consumer ownership, response pins, host prefill, pending fills, directory/full
bank-class reclaim and exact zero/default behavior remain required. Synthetic
expected51->17 lower requests is NOT an observed result and is not a workload
performance claim. Served-sector volume is not avoided DRAM traffic.

Remaining: E resource admission; actual build/native results; metadata control
rate/invariant acceptance; independent E qualification; D preregistered actual
reuse/input; E same-binary parent/clean pair within24 starts. Fixed-victim and
full dirty paths remain absent. No full application, new trace or orphan waiter.
