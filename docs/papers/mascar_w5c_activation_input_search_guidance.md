# Mascar W5C Guidance: Activation Input Search

## Stage Position

This is W5C: Activation Input Search for Table III workload coverage.

- Input: W5A/W5B activation matrix + subset manifest.
- Target: 6 ready workloads: bfs, spmv, mri_q, pathfinder, sgemm, stencil.
- Goal: Generate input / workload variants that trigger Mascar counters:
  - M2 MP cycles > 0
  - M2 owner acquire > 0
  - M3 hit-only attempt > 0
  - M4 re-exec enqueue/retry > 0
- Output: activation-ready subset manifest for W6/W7 full sweep.

## Tasks

1. Inspect current smoke input used in W4 for the 6 ready workloads.
2. Identify why counters were not triggered:
   - memory pressure too low
   - cache-sensitive data not aligned with L1 size
   - load/store distribution too low
   - small thread-block size
3. Modify input / workload parameters conservatively:
   - increase data size
   - increase active threads per SM
   - increase number of coalesced accesses
   - adjust memory-intensive regions
   - keep compute-intensive region as is for sanity
4. Update wrapper or command manifest to point to new input/data.
5. Run dry-run or small smoke:
   - verify wrapper runs without crash
   - collect preliminary counter stats
   - check if M2/M3/M4 counters are now nonzero
6. Iterate: tweak inputs until counters > 0 for at least one active configuration.
7. Do not run full benchmark yet; W6/W7 full sweep will run large-scale simulation.

## Required Outputs

1. experiments/paper-mascar/workloads/matrix/W5C/activation_ready_subset_manifest.csv
   - columns:
     - paper_id
     - wrapper_path
     - active_config
     - M2_active_counter_triggered (0/1)
     - M3_active_counter_triggered (0/1)
     - M4_active_counter_triggered (0/1)
     - input_modified (yes/no)
     - notes
2. experiments/paper-mascar/workloads/matrix/W5C/w5c_postcheck.md
   - start_iso
   - end_iso
   - elapsed_sec
   - branch/HEAD
   - git status before/after
   - commands run
   - counter results
   - notes on any debug/fix
3. docs/papers/mascar_w5c_activation_input_search_report.md
   - describe changes made
   - explain why inputs should now trigger M2/M3/M4 counters
   - recommendations for W6/W7 full sweep

## Hard Constraints

- Do not change Mascar mechanism.
- Do not modify M1–M4 configs.
- Only adjust input/data or wrapper/command parameters.
- Each iteration must be documented in notes.
- Debug any wrapper/run errors immediately.
- Do not run full benchmark.
- Do not submit W5C outputs.
- Do not use git add . or git add -A.
- Postcheck must use start_ts=$(date +%s) / end_ts=$(date +%s) for elapsed_sec.

## Validation

- All 6 ready workloads must attempt dry-run smoke.
- At least one active configuration per workload should trigger nonzero counters before finalizing activation subset.
- Record any failures or missing input.
