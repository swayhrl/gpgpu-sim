# Mascar W5B Guidance: Activation Matrix and Subset Workload Selection

## Stage position

This is W5B. Input: W5A activation counters.

Goal:

- Generate workload subset for full sweep (W6/W7)
- Rank workloads by expected Mascar mechanism activation
- Separate memory-intensive / compute-intensive
- Record which workloads likely trigger M2/M3/M4

Tasks:

1. Read activation_counters.csv from W5A
2. For each workload:
   - Mark M2 active if paper_mascar_m2_mp_cycles > 0
   - Mark M3 active if paper_mascar_m3_hitonly_access_attempt > 0
   - Mark M4 active if paper_mascar_m4_enqueue_success > 0
3. Combine flags into a single activation matrix:
   Columns: paper_id, paper_name, M2_active, M3_active, M4_active, total_counters_nonzero, type (Memory/Compute)
4. Rank workloads by total_counters_nonzero descending
5. Generate subset manifest for W6 full sweep
   - Only include workloads with at least one active counter
   - Include baseline_off config for reference
   - Include M4 active config for comparison
6. Produce a summary report:
   - number of active/inactive workloads
   - memory vs compute distribution
   - preliminary guidance on inputs (short/tiny/medium)
7. Postcheck file:
   - start_ts / end_ts / elapsed_sec
   - branch and HEAD
   - git status before/after
   - files changed
   - summary of active/inactive workloads
   - warnings

Required outputs:

- experiments/paper-mascar/workloads/matrix/W5B/activation_matrix.csv
- experiments/paper-mascar/workloads/matrix/W5B/subset_manifest.csv
- experiments/paper-mascar/workloads/matrix/W5B/w5b_postcheck.md
- docs/papers/mascar_w5b_activation_matrix_report.md

Validation:

- Ensure all 30 Table III rows appear in the activation_matrix.csv
- Confirm subset_manifest includes only active workloads
- Check consistency with W5A counters
- Confirm CSV headers and MD summary match required columns

