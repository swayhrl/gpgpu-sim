# Corrected L2 production-admission regressions

`run_synthetic.sh` compiles a deterministic regression against
`src/gpgpu-sim/l2_admission_rules.h`, the production header used by
`memory_sub_partition::cache_cycle()` and
`memory_partition_unit::can_issue_to_dram()`.  It does not copy the admission,
DRAM-issue, miss-rate, or preview/commit predicates.

The tests cover the resource contracts T1–T15 in the corrected-baseline
handoff: exact frontend admission, writeback issue while the return path is
full, windowed sector-miss arithmetic, and the distinction between new-MSHR
and merge capacity.  The production controller checks every accepted QV100
access against its preview (events and MissQ delta) in every build and exports
`L2_char_preview_commit_mismatch`; the integrated P1--P6 harness requires it
to remain zero.

Run:

```bash
tests/l2_char/run_synthetic.sh
```

The Instrumentation v1 production fixtures use the real
`memory_partition_unit`/`memory_sub_partition` path and are intentionally
small enough for closeout use:

```bash
tests/l2_char/run_set_all_reserved.sh       # C5: all ways reserved -> LINE_ALLOC_FAIL
tests/l2_char/run_mshr_lifetime.sh          # C6: new miss + merge + held response drain
tests/l2_char/run_fill_port_contention.sh   # C8: DRAM->L2 fill-port contention
tests/l2_char/run_rop_input_full.sh         # C9: ready ROP blocked by ICNT->L2
tests/l2_char/run_memory_partition_returnq.sh # C10/P5A: ReturnQ issue causality
```

They are verification fixtures only: all directed holds are default-off and
operate at an existing production boundary.  They neither alter normal
admission nor authorize workload characterization sweeps.
