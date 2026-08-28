# L2CHARV1 resource model

`L2_bank_xxx` denotes a simulator L2 slice / memory subpartition, not a
physical SRAM bank.  The primary observed resources are replacement-way
reservation, MSHR entries, per-entry MSHR targets, the shared MissQ, fill-path
capacity, WB-path traffic, and data-port bandwidth.

Each `memory_sub_partition::cache_cycle()` samples after response/fill and
MissQ drain work and before the current ICNT→L2 frontend request is admitted.
The sample count is consequently an L2 cache-cycle count for that slice.

The baseline has no standalone WBQ or physical tag/data-bank model.  V1 names
writeback occupancy as MissQ/L2→DRAM WB-path occupancy and never reports a
WBQ or physical SRAM-bank utilization.
