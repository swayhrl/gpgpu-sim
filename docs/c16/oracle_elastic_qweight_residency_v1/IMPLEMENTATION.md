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

## Frozen-contract fail-closed boundary

The frozen rules do not define a legal action when the per-instance quota is full but the addressed set contains an invalid line or contains no eligible protected victim. A set-associative cache cannot evict a protected line from another set without changing mapping, lifetime, or baseline replacement semantics. The implementation therefore terminates explicitly with `SELECTION_FROZEN_CONTRACT_UNREPRESENTABLE`; it never silently overcommits, admits the target as ordinary, changes queueing, or performs cross-set eviction.

This boundary must be resolved or proven unreachable by trace/address integration before real C16 simulation. Consequently the implementation is preparation evidence, not a C16 performance result.
