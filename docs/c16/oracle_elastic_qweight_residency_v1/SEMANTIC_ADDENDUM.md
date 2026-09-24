# Semantic addendum: quota-full set-local admission denial

This addendum closes the V1 state omitted by the original specification. It does not change the oracle identity or introduce another replacement mechanism.

`target-tagged` means eligible to request protected admission. A fill becomes protected only when the hard per-instance quota and set-local baseline replacement constraints permit it.

At quota full:

1. If baseline has an invalid candidate, preserve the exact baseline invalid choice and admit the target unprotected (`QUOTA_FULL_BASELINE_INVALID_PRIORITY`).
2. With no invalid candidate and an eligible protected candidate, select protected by baseline LRU/FIFO and transfer protection.
3. With no invalid or eligible protected candidate but an eligible ordinary candidate, select the baseline ordinary victim and admit unprotected (`QUOTA_FULL_NO_LOCAL_PROTECTED_VICTIM`).
4. With no baseline-eligible candidate, return the same `RESERVATION_FAIL`.

An admission-denied fill creates no pending protected reservation. It is never promoted by a later target hit and its eviction never decrements protected occupancy.

Forbidden alternatives remain: temporary overcommit, cross-set victim selection, global demotion, target stall, per-set quotas, changed associativity, or changed MSHR/queue/dirty eligibility.
