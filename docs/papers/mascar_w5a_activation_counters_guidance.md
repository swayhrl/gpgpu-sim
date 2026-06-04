# Mascar W5A Guidance: Mechanism Activation Counters and Preliminary Smoke

## Stage position

This is W5A. 

- Input: W3/W4 smoke pass results, W1/W2 workload inventory, Table III 30 workloads.
- Goal: Identify which workloads trigger M2/M3/M4 counters.
- Scope: Use small or existing smoke workloads. Do not run full benchmark.
- Output: per-workload activation counters, preliminary classification.

## Activation Counters

Collect the following per workload/config:

- M2:
  - paper_mascar_m2_mp_cycles
  - paper_mascar_m2_owner_acquire
  - paper_mascar_m2_nonowner_mem_block
- M3:
  - paper_mascar_m3_hitonly_access_attempt
  - paper_mascar_m3_hitonly_access_hit
  - paper_mascar_m3_hitonly_access_nack
- M4:
  - paper_mascar_m4_enqueue_success
  - paper_mascar_m4_retry_attempt
  - paper_mascar_m4_retry_hit
  - paper_mascar_m4_retry_nack
  - paper_mascar_m4_retry_requeue

Conditions:

- Consider M4 active configuration (gpgpu_mascar_enable_reexec_queue=1)
- If a workload’s counters remain zero, mark “inactive”.
- If counters nonzero, mark “active”.

## Required directories

- experiments/paper-mascar/workloads/matrix/W5A/
- experiments/paper-mascar/workloads/results/W5A/

## Tasks

1. Run smoke with baseline_off and M4 active configs.
2. Parse logs to extract M2/M3/M4 counters.
3. If active counters nonzero, record in per-workload table.
4. Record inactive workloads separately.
5. Document failures or incomplete runs (timeout, crash, missing input).

## Outputs

- experiments/paper-mascar/workloads/matrix/W5A/activation_counters.csv
- experiments/paper-mascar/workloads/matrix/W5A/activation_summary.md
- experiments/paper-mascar/workloads/matrix/W5A/w5a_postcheck.md

