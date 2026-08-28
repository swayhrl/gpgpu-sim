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
