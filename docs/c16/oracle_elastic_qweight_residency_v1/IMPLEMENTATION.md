# ORACLE_ELASTIC_QWEIGHT_RESIDENCY_V1 implementation note

The implementation is opt-in and defaults OFF. It changes only L2 victim preference and protected metadata/lifetime. It does not modify simulated latency, ports, MSHRs, merging, memory ordering, associativity, line size, dirty eligibility, or queues.

Configuration:

- `-gpgpu_l2_oracle_elastic_qweight_enable 0|1`
- exactly one of `-gpgpu_l2_oracle_elastic_protected_quota_bytes` or `-gpgpu_l2_oracle_elastic_protected_quota_lines`
- `-gpgpu_l2_oracle_elastic_interval_sidecar PATH`
- `-gpgpu_l2_oracle_elastic_sidecar_sha256 LOWERCASE_SHA256`
- `-gpgpu_l2_oracle_elastic_diagnostics 0|1`

The global line budget is `ceil(bytes / line_bytes)` or the exact configured line count. Quotient/remainder allocation over the actual L2 instance count preserves the global sum exactly.

Protection is committed only when a reserved line fills. Pending protected reservations are tracked separately so concurrent misses cannot over-admit the quota; the published occupancy invariant remains the count of resident non-invalid protected lines. Eviction, whole-line invalidation, flush and reset clear metadata exactly once. Sector misses retain line-level protection and never promote an ordinary line.

## Authorized admission-denial addendum

A target tag means eligibility to request protected admission, not a guarantee of protection. When the per-instance quota is full:

- baseline invalid priority is preserved; the target fill uses the baseline invalid candidate and remains unprotected;
- otherwise, an eligible local protected victim is selected by baseline LRU/FIFO and protection transfers to the incoming fill;
- if no local protected victim exists, the baseline set-local ordinary victim is selected and the incoming fill remains unprotected;
- if baseline has no eligible candidate, the baseline `RESERVATION_FAIL` remains unchanged.

Denied admissions create no pending reservation and cannot change occupancy. Diagnostics distinguish `QUOTA_FULL_BASELINE_INVALID_PRIORITY` from `QUOTA_FULL_NO_LOCAL_PROTECTED_VICTIM`. There is no quota overcommit, cross-set eviction/demotion, global victim directory, target stall, promotion, per-set quota, or queueing change.

The mechanism remains implementation-preparation evidence, not a C16 performance result. Real C16 replay still requires bounded-trace and `TRACE_ADDRESS_NAMESPACE -> L2 get_addr()` admission.
