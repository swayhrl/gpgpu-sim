# L2CHARV1 set-state accounting

L2CHARV1 reports **way-level** replacement pressure, not physical SRAM-bank
activity.  A sector-cache way is counted as reserved when the existing
`cache_block_t::is_reserved_line()` production predicate is true: at least one
sector is in `RESERVED`, which is also the condition that makes the way
unavailable to the replacement search.  Dirty and valid use the corresponding
production `is_modified_line()` and `is_valid_line()` predicates; mixed-sector
ways therefore count once at way granularity.

The tracker is opt-in and is attached only to an L2 instance when
`-gpgpu_l2_char_enable 1` is selected.  It is held in an external map keyed by
`tag_array *`, so enabling the instrumentation does not change the shared
L1/L2 `tag_array` object layout.  It performs one bounded initialization scan
when the L2 is created, then refreshes the affected way after each real tag
state transition (access, fill, atomic completion, flush, or invalidate).
Per-cycle sampling reads only the maintained totals and per-set reservation
counts; it does not scan all sets or ways.  This preserves the sampling point
while keeping host overhead bounded.
