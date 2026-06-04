# Mascar M5B Guidance: Focused Validation Matrix and Result Collection

## Stage position

This is M5B.

M5A discovered runtime environment and attempted smoke/debug.
M5B builds a focused validation matrix and runs it if possible.
M6 will write the final report from the collected data.

## Goal

Validate the incremental effect of Mascar mechanisms:

- baseline
- old proxy Mascar if available
- M1 L1 saturation probe only
- M2 owner scheduling
- M3 non-owner hit-only / NACK
- M4 load-only re-execution queue

The goal is not to run the full paper benchmark suite. The goal is to confirm:
- active configs run
- Mascar stats are nonzero on memory-stressing workloads
- no obvious deadlock
- results are explainable

## Required config matrix

Create:

- experiments/paper-mascar/m5_config_matrix.csv

Columns:

- config_id
- config_path
- purpose
- gpgpu_enable_mascar
- l1sat_probe
- m2_owner_sched
- m3_hitonly_nack
- m4_reexec
- old_proxy_sched
- expected_behavior

Include at least:

1. baseline_off
2. m1_l1sat_probe
3. m2_owner_sched
4. m3_hitonly_nack
5. m4_reexec_load

Optionally include:
6. old_proxy_mascar
7. m4_reexec_probe_only

If a baseline/off config does not exist, create:

- configs/hrl-repro/SM7_QV100_mascar_baseline_off/

It should copy an existing compatible SM7_QV100 config and set:
- -gpgpu_enable_mascar 0
- all Mascar active knobs off if present

Add README explaining it is the baseline/off comparator for focused validation.

## Required workload manifest

Create:

- experiments/paper-mascar/m5_workload_manifest.csv

Columns:

- workload_id
- suite
- type
- command
- working_dir
- expected_runtime
- status
- notes

If runnable workloads are found in M5A:
- add 1 to 4 focused workloads.
- prefer memory-intensive or cache-sensitive workloads.
- keep each run under 20 minutes.
- include exact command and working directory.

If no workloads are available:
- create the CSV with headers and example commented guidance in a companion README.
- status should be unavailable.
- do not invent commands.

Create companion:

- experiments/paper-mascar/m5_workload_manifest_README.md

Explain how a user should fill commands if external benchmark env is later provided.

## Runner script

Create:

- experiments/paper-mascar/run_m5_focused_validation.sh

Requirements:

- bash script
- set -euo pipefail is acceptable, but handle individual run failures and continue to next run.
- accepts optional env variables:
  - M5_CONFIG_MATRIX
  - M5_WORKLOAD_MANIFEST
  - M5_OUTDIR
  - M5_TIMEOUT_SEC
  - M5_MAX_RUNS
- default outdir:
  experiments/paper-mascar/m5_runs/YYYYMMDD_HHMMSS
- source setup_environment release if available.
- for each config/workload pair:
  - create per-run directory
  - copy gpgpusim.config and config_volta_islip.icnt into run dir
  - run workload command from working_dir or run dir
  - use timeout
  - save stdout/stderr/log
  - save exit code
  - grep stats into stats.txt
  - write run row into run_manifest.csv
- do not run if workload manifest contains no available workloads.
- print clear instructions if no workload exists.

Important:
  The script should be useful even if not run in this round.

## Collector script

Create:

- experiments/paper-mascar/collect_m5_results.py

Requirements:

- Python 3.
- Input: M5 run directory.
- Parse each stats.txt or simulator log.
- Extract:
  - gpu_tot_sim_cycle
  - gpu_tot_ipc
  - paper_mascar_l1_sat_sample
  - paper_mascar_l1_sat_sample_saturated
  - paper_mascar_m2_ep_cycles
  - paper_mascar_m2_mp_cycles
  - paper_mascar_m2_owner_acquire
  - paper_mascar_m2_nonowner_mem_block
  - paper_mascar_m3_hitonly_access_attempt
  - paper_mascar_m3_hitonly_access_hit
  - paper_mascar_m3_hitonly_access_nack
  - paper_mascar_m4_enqueue_success
  - paper_mascar_m4_retry_attempt
  - paper_mascar_m4_retry_hit
  - paper_mascar_m4_retry_nack
  - paper_mascar_m4_retry_requeue
  - paper_mascar_m4_queue_occupancy_max
  - any cache hit-rate lines if easy
- Output:
  - experiments/paper-mascar/m5_results.csv
  - experiments/paper-mascar/m5_results_summary.md

If no logs exist:
- produce an empty CSV with headers.
- summary must state results were not collected due missing runtime workload.

## Running the matrix

If M5A found runnable workloads:

1. Run:
   bash experiments/paper-mascar/run_m5_focused_validation.sh

2. Run collector:
   python3 experiments/paper-mascar/collect_m5_results.py <run_dir>

3. Inspect failures.
4. If a run fails due code/config bug:
   - debug and fix
   - rerun that failing subset
   - update m5_results.csv

If no runnable workloads:
- do not run full suite.
- verify scripts with:
  bash -n experiments/paper-mascar/run_m5_focused_validation.sh
  python3 -m py_compile experiments/paper-mascar/collect_m5_results.py

## Expected result interpretation

If runs complete:

- baseline should have no active paper_mascar_m2/m3/m4 behavior, or all zero.
- M1 config should show l1_sat samples if memory pressure is seen.
- M2 config should show owner/MP stats if saturation occurs.
- M3 config should show hit-only attempts/NACKs if non-owner loads occur in MP.
- M4 config should show enqueue/retry stats if NACK or reservation fail occurs.
- If stats are zero, do not call it failure automatically:
  workload may not cause saturation.
  record as "workload did not activate mechanism."

If M4 active crashes or deadlocks:
- debug and fix in this round if feasible.
- rerun.
- record fix.

## Required M5B outputs

- experiments/paper-mascar/m5_config_matrix.csv
- experiments/paper-mascar/m5_workload_manifest.csv
- experiments/paper-mascar/m5_workload_manifest_README.md
- experiments/paper-mascar/run_m5_focused_validation.sh
- experiments/paper-mascar/collect_m5_results.py
- experiments/paper-mascar/m5_results.csv
- experiments/paper-mascar/m5_results_summary.md

## Stop conditions

Stop only if:

1. implementation bugs require broad M1-M4 redesign.
2. runner causes destructive changes outside experiments/paper-mascar.
3. build cannot be restored.
4. elapsed time exceeds 150 minutes.

Do not stop because one test fails. Debug and continue.
