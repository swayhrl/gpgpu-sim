# Mechanism study A source candidate, first diagnostic revision

Parent84359d4ba492acb639a2092fa271e9169038bf7d. New control authority Framework
d9263fa2431592b33e900d9b48c6f20029d8b544. This is source plus finite regression
PREPARATION, NOT_COMPILED_NOT_RUN. Parent qualifications are not inherited.

Defaults: diag_copy_free=0, diag_l2_data_free=0, ms_observer=0. Requested profiles
are S, U, S_COPY_FREE, S_L2_DATA_FREE, U_L2_DATA_FREE, with observer0/1. New
diagnostics reject each other and S64/quota/P2/MQ2/RG1/old ideals; existing
unimplemented RO/TVD rejection remains. No extra data/descriptor/directory pool.

COPY_FREE preserves actual phase0 snapshot and phase1 WBUF install/role change,
but exempts both COPY clients from physical service and removes their dedicated
copy staging delay. LOWER_WB readout stays physically charged. DATA_FREE exempts
payload service clients from bank contention but retains the one-cycle fixed
copy-latch transfer. Both retain ROP, frontend/queue width, native Tag/MSHR/WAD/
descriptor/landing limits, miss generation, lower service, pins and completion.
Logical exempt counts are separate from unchanged physical bank grants/peaks.

Bounded observer: one current frontend UID, per-reason first wait and first
observed unblock, 65 log2 bins, censored/unknown checks retained. A masked check
does not imply resource availability. Snapshot precedes frontend mutation;
actual bank denial/copy phase outcomes augment that same attempt once. All
durations are physical GPU cycles. They overlap across reasons and slices.
`lower_*_bytes` counts native packet data_size at accepted lower enqueue,
excluding protocol headers; it is not DRAM burst bytes or completed-read time.
WB role occupancy includes reserved/staged slots; WAD survives local release.
Physical resident-role count is not nominal Tag occupancy. Pending landings
count actual RESIDENT_FILL_PENDING or pending-sector slots, once per slot.

Opt-in native targets (none runs on configure/default build):

- ep_l2_ms_local_service: actual small cache, two cold sectors in reverse return
  order, pinned write refusal, hot read, full copy values, WB-only local release
  with WAD held, old packet snapshot after actual static staging reuse/drain.
- ep_l2_ms_partition: actual1024Tags/1168payloads/four banks, nonzero host prefill
  offset, fill/frontend contention, held response FIFO, real cold lower reads,
  hot hit, exactly-once symbolic values/drain; S capacity variant holds16local
  WB slots plus one accepted lower packet and retries one blocked UID64+cycles.
- ep_l2_ms_observer: standalone native arithmetic/identity/known-mask unit,
  deliberately not a substitute for production partition observer replay.
- ep_l2_ms_mode_guard: forked real constructor rejection with exact SIGABRT and
  configuration guard text, no core dump.

Framework entry also schedules affected old all-OFF, physical clock0/4096,
host-prefill, stale return, sector atomic/value/WB cases, plus affected diagnostic
atomic variants. Native symbolic versions are not numerical application proof.
Small fixtures use their documented geometry; H0 application geometry and all
non-research settings stay frozen in E's separate matched configs/canary.

Actual compilation/execution only after E's exact current resource admission,
48GiB workspace plus2GiB build reserve, Share40GiB, MemAvailable64GiB and central
global leases. No simulator application, new background waiter, migration or
old-data cleanup is part of this source candidate. E independently qualifies
each exact built revision and launches all full applications within24 starts.
