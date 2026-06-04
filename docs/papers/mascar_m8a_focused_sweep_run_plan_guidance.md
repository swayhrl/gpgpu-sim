# Mascar M8A Guidance: Focused Sweep Run Plan

## Stage

This is M8A.

Input:
- W7 activation-ready subset (spmv, mri_q, pathfinder)
- W7 postcheck and counters

Goal:
- Generate W8 run plan and subset manifest
- Prepare CSV for focused sweep
- Preserve placeholders/unavailable rows
- Mark which workloads are ready for baseline/M2/M3/M4 configurations

Tasks:
1. Read W7 activation-ready subset manifest and notes
2. Construct W8 config matrix:
   - baseline_off
   - m2_owner_sched
   - m3_hitonly_nack
   - m4_reexec_load
3. Map each workload to all enabled configs
4. Assign run_priority:
   - activation-ready first
   - ready but not yet active
5. Record expected activation flags (0/1)
6. Generate:
   - w8_run_plan.csv
   - w8_workload_manifest.csv
   - w8_postcheck.md
