# C16 old-address generation survival observer V1

Status: `C16_OLD_ADDRESS_SURVIVAL_OBSERVER_IMPLEMENTATION_QUALIFIED`.

The observer is default OFF and diagnostics-only. Cache code sends one-way
facts—accepted new-line allocation, completed first-sector fill, whole-line
eviction/invalidation, true valid-sector hit, and kernel boundary. The observer
returns no value and is never consulted by probe, victim, admission, queue,
port, or timing logic.

External state maps each aligned 128-byte address to a monotonically increasing
fill-generation serial and current resident/protected/class identity. A new
generation is created only when the first fill completes after a new-line
allocation; later sector-child fills do not create generations.

For the qualified full trace, launch UID 60 is dynamic kernel 2985 and freezes
the D1 L0 protected-class-1 snapshot. UID 1564 is dynamic kernel 4489 and emits
the D2 L0 pre-range state. UID 1565 is dynamic kernel 4490 and emits mid/end
state. These mappings are frozen in the supplied template config.

At each report, snapshot addresses close exactly into unchanged old-generation
survivors, currently resident new generations, and currently absent addresses.
`evicted_then_refilled_same_address` is a cumulative address count and is not
substituted for this disjoint closure. The strictly defined reload counter is
a completed new-generation fill for a snapshot address while D2 observation
is active. Request-level “misses” are intentionally not emitted because MSHR
merge and sector behavior prevent an unambiguous one-to-one definition.

Observer qualification demonstrates implementation correctness and OFF/ON
neutrality. It is not a scientific survival result; no full window was run.
