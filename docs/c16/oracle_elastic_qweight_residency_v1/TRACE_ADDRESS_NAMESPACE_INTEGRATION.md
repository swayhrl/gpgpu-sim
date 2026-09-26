# Trace address namespace integration observer

This integration-only extension is based on `ORACLE_ELASTIC_QWEIGHT_RESIDENCY_V1` at `a7d3c2bed3f8455ec372040e425b890ba290621b`.

It adds two default-off diagnostic options:

- `-gpgpu_l2_oracle_address_canary_log PATH`
- `-gpgpu_l2_oracle_address_canary_target_limit N`
- `-gpgpu_l2_oracle_address_canary_exact_address ADDRESS`

When configured, the observer records trace-parser, instruction-operand, coalesced `mem_access_t`, `mem_fetch`, L2-entry and oracle-lookup addresses. A nonzero target limit permits a successful early exit only after the exact address reaches `ORACLE_LOOKUP` with an interval hit. This mode is labeled `ADDRESS_INTEGRATION_CANARY_ONLY` and is not a performance run.

The observer is absent from victim selection and all options default to disabled. Existing baseline-OFF and diagnostics-neutrality tests remain unchanged.

## Sector-path repair discovered by real canary

The first real-artifact run exposed that `oracle_commit_fill()` cleared an existing protected bit on a sector miss when no new pending admission existed. The occupancy counter correctly remained unchanged, so the full-array invariant detected the mismatch. The repair leaves metadata unchanged when `pending_protected == false`; new ordinary allocations are already cleared by the preceding depart/allocate path. A directed `tag_array` sector-fill regression now locks this behavior.

No quota, admission-denial, replacement, promotion, lifetime, associativity, MSHR, queueing or timing semantic changed.
