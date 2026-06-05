# Mascar W15B Guidance: Table III M3 Diagnostic Runs

## Stage position

W15B uses W15A counters to diagnose existing Table III ready workloads.

## Goal

Run diagnostic configs on current candidate workloads and determine where M3 activation is blocked.

Primary workloads:
- spmv
- mri_q
- pathfinder
- bp_2
- srad_1
- srad_2
- bp_1

Also include any ready memory-intensive workload found by W10-W14.

## Required configs

Run at least:

1. m4_reexec_load
2. m3diag_on
3. m3diag_forced_mp_on

If runtime is long, start with:
- spmv
- pathfinder
- bp_2
- srad_1

Then expand.

## Iteration policy

W15B must not stop after one failed attempt.

For each workload:
- attempt normal diagnostic config
- if M3 still zero, attempt forced-MP diagnostic config
- if still zero, inspect skip counters
- if skip counters show an obvious config/wrapper issue, fix and rerun
- if skip counters show scheduler blocks non-owner load before LSU, mark possible implementation bug and proceed to W15C/W15D

At least 2 diagnostic attempts are required before marking a workload unresolved.

## Required outputs

Create:

- experiments/paper-mascar/workloads/matrix/W15B/w15b_config_matrix.csv
- experiments/paper-mascar/workloads/matrix/W15B/w15b_workload_manifest.csv
- experiments/paper-mascar/workloads/matrix/W15B/run_w15b_m3_diagnostic.sh
- experiments/paper-mascar/workloads/results/W15B/
- experiments/paper-mascar/workloads/results/W15B/w15b_latest_results.csv
- experiments/paper-mascar/workloads/results/W15B/w15b_latest_summary.md
- experiments/paper-mascar/workloads/results/W15B/w15b_latest_status_matrix.csv
- experiments/paper-mascar/workloads/results/W15B/w15b_latest_run_manifest.csv
- docs/papers/mascar_w15b_tableiii_m3_diagnostic_report.md
- experiments/paper-mascar/workloads/audit/W15/w15b_postcheck.md

## Diagnostic classification

For each workload/config classify:

- m3_activated
  hitonly probe called or m3 attempt > 0

- blocked_before_lsu
  non-owner load candidate exists but scheduler blocks before LSU

- no_mp
  skip_not_mp dominates

- no_owner
  skip_no_owner dominates

- no_nonowner_load
  no nonowner load candidates

- config_off
  skip_config_off dominates

- l1_probe_no_hit
  probe called but no hit

- unknown

## Required summary

Create:

- experiments/paper-mascar/workloads/matrix/W15B/w15b_m3_diagnostic_summary.csv

Columns:
- paper_id
- config_id
- result_status
- m2_active
- m3_active
- m4_active
- m3diag_nonowner_load_candidate
- m3diag_scheduler_allow_nonowner_load
- m3diag_skip_not_mp
- m3diag_skip_no_owner
- m3diag_skip_owner_warp
- m3diag_skip_not_load
- m3diag_skip_m2_blocked_before_lsu
- m3diag_hitonly_probe_called
- m3diag_hitonly_probe_hit
- m3diag_hitonly_probe_nack
- diagnosis
- next_action

## Validation

Run:
- bash -n run_w15b_m3_diagnostic.sh
- dry-run first
- actual diagnostic runs with timeout
- collect results
- produce summary

## Stop conditions

Stop only if:
1. all diagnostic configs fail to run due build/runtime error that cannot be fixed.
2. results are missing and cannot be collected.
3. elapsed time exceeds 150 minutes.

Do not stop because M3 remains zero. Diagnose why.
