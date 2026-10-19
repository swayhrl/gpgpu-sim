# A-integrated RO source candidate r1

Status: SOURCE_PREPARED_NOT_COMPILED_NOT_RUN. No binary, native PASS,
application, independent E acceptance, speedup or PPA claim.

Parent A Core17e7e224954e44a968a0447bcdfea91e3b2e5a96. Consume only frozen
C Coreb1d42152805e22b6a12774f37351926b6fcbeaf5 and Framework receipt
 d98ffab2f27d07f8b20f6c8ea63dcb4091c4c78b; A44 adapter SHA256
37ce97cba3ec5d427ea63d67c3cc82ea8bde45ab6232704d81b117d51e4276c0.
No whole historical merge. R1 and CLEAN_ONLY remain separate frozen candidates.
No TVD/RO combination is exposed in this branch.

## Mechanism and cost boundary

Four cells T/M00,10,01,11 share U16/P1/D512/native128/address32/WAD128,
1168x128B payload,1024 production Tags, four physical banks. Tests use reduced
Tag geometry to create bounded pressure; application overlays keep1024 Tags.
T-only retains native line records after detach; M-only keeps its Tag; T+M may
have independent same-address lower transactions after detach. Unknown, atomic,
write, synthetic and noncertified reads stay native; any writer to a certified
line fails the certificate. DefaultOFF allocates no RO runtime or certificate.

Each live RO waiter uses the real native D512 free-list; per-address32 counts
native+RO. One completion control entry per existing physical landing (1168
maximum) holds identity, canonical line/epoch, Tag identity+uint64 generation,
required/issued/ready4-bit masks, prepared response metadata and chains of shared
descriptor identities only. Chains have at most512 live members in total;
there is no second pointer/sector/request pool or data RAM. Shared descriptors
add RO-external, owner line and retained uint64 generation. Per-Tag generation
is uint64; payload GEN32 is unchanged. Role/state labels require at least3/4
bits. The packet adds one source bool RO-lower discriminator, using the existing
payload ID/GEN fields for completion; no new payload bytes. Exact ABI, wire
packing, gate area/timing are NOT measured. New metadata is not free and old
R3 PPA does not apply.

One frontend ticket per slice: actual head issues a bounded descriptor/Tag
query, max1 per physical cycle, ready+1. Per-address checks scan existingD512;
this declares parallel comparison/control rather than a free second CAM/index.
Frontend preview is immutable. Response-ready selection scans the finite1168
control entries/at most512 descriptor chains; the original P1 arbiter still
charges every accepted RO fill WRITE/response READ. Fixed pipelines, queues,
ROP, lower memory, order and conservative sticky reuse protection remain.
A denied response rereads on a later grant; no hidden response data buffer.
Additional storage/read/control-port realization requires cost review, not a
single-port SRAM assertion. Global full-app closure must still pass.

## Concrete integration corrections

- Keep one virtual out-of-line l2_cache destructor; the supplied public patch
  otherwise leaves both the original inline and new out-of-line declarations.
- Define the RO runtime before l2_cache::access/preview dereference its type.
- Register all T/M and hash/scope/file CLI options; reject unsupported mixes.
- Bind GPU/partition/cache APIs to the actual whole commandlist. Disabled RO
  GPU and partition wrappers return before touching an absent disabled L2.
- Runtime Framework starts once after initial HtoD prefix, before first kernel;
  spans all kernels, drains before subsequent memcpy, closes only at complete
  uncapped commandlist+backend drain. A capped run cannot print COMMANDLIST_END.
  OFF keeps native command timing. Fix the existing string-constructor member
  shadow so the new interface also has an initialized GPU member on that path.
- Frontend lookup executes only on the true L2-eligible branch; RO enabled
  rejects L2-disabled/texture-only routing rather than certifying a bypass.
- RO accepted lower bytes feed the common observer before the RO callback.
  Custom lower packets are created in the cache and bypass frontend push(),
  hence are never in m_request_tracker; their deletion needs no tracker erase.
  Original requesters remain tracked until their actual ICNT pop.
- Final RO summary includes separate native/RO descriptor completions and
  shared occupancy; old native-only subcounters retain their meaning. Global
  partition exact-once/DRAM/ICNT/ownership gates cannot be waived. E must adapt
  RO qualification to require commandlist begin/end and RO closure; its old
  diagnostic terminal parser alone is not new RO acceptance.
- Source-extracted bank fixture reads RO MSHR definitions from their .inc, not
  the old containing .cc. It is a facade test, separate from real cache and
  partition fixtures; it needs its explicit source-preparation/build step.

## Frozen oracle and runtime qualification

C reviewed D ca1ca06bd3e9384298b18bba5dbd68a8d9d8295b Conv whole-epoch
certificate: manifest1cb3bc004fa7824e2e9eb3b1da991b8114fb588eae8e87cf92e124996b7bd425,
ranges a8dfd6d20ef62f959bd7908abf76b58434039ae63e611059c490a8c5c177e80e,
294912 lines. Reuse C/E independent artifact reviews, not a fresh trace-body
rehash by A. These prove closed-world input preparation only: full commandlist,
all visible agents, no external writers/aliases, prior drain and memcpy at
drained boundaries. E must freshly bind full inputs/oracle before launch.
Neither certification nor synthetic sector versions prove numerical SASS
outputs or Tag/MSHR pressure/speedup.

## Required finite execution, all pending

Build this exact Core with its separate A RO runtime Framework, same CUDA11.8,
assertions enabled and vendor f90c430c3a1ce8f08c09942a5a1c3a797bb2295b.
The R1 build entry is intentionally still R1; it cannot compile this candidate
by changing its working tree. No RO build is admitted yet. A proposes a fresh
serial j2/8GiB build, up to3GiB retained build growth plus128MiB result growth;
E must budget/freeze an exact source/builder/output lease and all48/40/64GiB
floors, including existing reserves. This is a conservative proposal, not a
measured new peak or an exception. Do not reuse/overwrite an R1 build tree.

Opt-in CMake targets: ep_l2_ro_runtime, ep_l2_ro_partition,
ep_l2_ro_transactions, ep_l2_ro_store_port, ep_l2_ro_oracle_certificate,
ep_l2_ro_epoch_entry, plus affected R1 native targets and accel-sim.out BUILD.
No configure target runs tests. Use C run_ro_runtime.py for its frozen finite
runtime/partition/hash/epoch/command rejection cases; source hashes must match
this exact integrated commit. Run module transactions/store/oracle and the four
GPU-wide prefix/bind/begin/end cases. Source-extracted bank A2/A3/grant tests
need current exact source bindings, not an inherited PASS. Then the affected
R1 OFF/diagnostic/host-prefill/physical-clock/atomic/version set on this new ABI.
E alone runs any whole-app canary/four cells within24 actual-start budget.

A integration finite runner enumerates22 child native processes: runtime1,
partition4, warmup/clock partition4, GPU epoch4, module3, texture-only refusal1,
certificate hash/scope/epoch refusals4, commandlist refusal1. The duplicate
native OFF process in C's original runner is assigned to the mandatory affected
R1 common suite instead; do not repeat a passed identical OFF test. Three
source-extracted facade processes (a2/a3/controls, C16 only) are separate.
Thus25 additional RO processes are prepared; actual compiler/native count0.
The whole-Framework driver hook itself still needs admitted compilation and
E's independently accepted whole-commandlist canary, never a unit-test waiver.
