# Mascar W7 Guidance: Stronger Activation Input / Memory Pressure Search

## Stage Position

This is W7 of the Mascar Table III workload coverage effort.

- Input: W6 activation-ready run plan and W5C subset manifest.
- Goal: Adjust inputs/data/block/thread configurations of ready workloads to trigger Mascar M2/M3/M4 counters (MP cycles, owner acquire, hit-only/NACK, re-exec enqueue/retry).
- Output: W7 activation-ready subset manifest for W8 full sweep.

## Target Workloads

- 6 ready workloads from Table III: bfs, spmv, mri_q, pathfinder, sgemm, stencil.
- Memory-intensive workloads first priority (bfs, spmv, pathfinder, stencil).
- Compute-intensive workloads only for sanity (do not over-stress).

## Tasks

1. Inspect current W6 ready workloads.
2. Analyze why M2/M3/M4 counters remained zero:
   - data size too small
   - memory intensity too low
   - thread block size too small
   - coalesced memory patterns too low
3. Modify workload wrapper / input:
   - Increase dataset size
   - Increase active threads per SM
   - Increase number of coalesced memory accesses
   - If feasible, adjust block/grid dimensions to stress memory
4. Run dry-run or small smoke for each modified input:
   - Collect M2/M3/M4 counters
   - Verify wrapper runs correctly
5. Iterate until at least one workload triggers non-zero counters in any config (M2/M3/M4)
6. Record notes/debug for every iteration
7. Generate W7 activation-ready subset manifest with columns:
   - paper_id, wrapper_path, active_config
   - M2_active_counter_triggered (0/1)
   - M3_active_counter_triggered (0/1)
   - M4_active_counter_triggered (0/1)
   - input_modified (yes/no)
   - notes
8. Update postcheck and report:
   - experiments/paper-mascar/workloads/matrix/W7/w7_postcheck.md
   - docs/papers/mascar_w7_activation_input_search_report.md
   - experiments/paper-mascar/workloads/matrix/W7/activation_ready_subset_manifest.csv
9. Do not run full benchmark yet; only small smoke / dry-run.

## Hard Constraints

- Do not modify M1–M4 Mascar mechanism.
- Do not fetch upstream.
- Do not create a new branch.
- Debug wrapper/runner/config issues if smoke fails.
- Keep all placeholder/unavailable Table III rows represented in manifests.
- Postcheck must use start_ts / end_ts for elapsed_sec.
- Do not submit W7 outputs.
- Do not use git add . or git add -A.

## Validation

- All ready workloads must be attempted with dry-run or small smoke.
- At least one active counter non-zero before finalizing W7 subset.
- Record activation-ready subset even if partial (some workloads still inactive).
- Document every change of wrapper/input.
