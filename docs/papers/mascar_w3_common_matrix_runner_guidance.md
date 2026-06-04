# Mascar W3 Detailed Guidance: Common Matrix Runner and Collector Upgrade

## Stage position

This is W3 of the Mascar workload coverage effort.

Completed:
- Mascar M1-M4 mechanism implementation.
- M5/M6 focused validation on rodinia_hotspot.
- W1/W2 Table III workload inventory, command manifest, and wrappers.

W3 must upgrade the workload infrastructure into a reusable matrix runner and collector that can be reused for Mascar and later papers.

W4 will use this runner to run a Table III baseline + M4 active smoke pass.

## Goals

1. Build a generic GPGPU-Sim config x workload runner.
2. Build a generic stats collector.
3. Keep compatibility with W2 wrappers.
4. Correctly classify wrapper placeholders and no-explicit-pass results.
5. Avoid treating any nonzero exit as "pass" just because a log exists.
6. Preserve enough logs and manifests for GPT review.
7. Keep the framework reusable for future CCWS/DAWS/PCAL or user experiments.

## Hard constraints

1. Do not modify Mascar mechanism code unless a build/runtime issue clearly requires a small fix.
2. Do not run full benchmark suites in W3.
3. Do not fetch upstream.
4. Do not create a new branch.
5. Do not discard W1/W2 manifest rows.
6. Do not treat hotspot as a Table III workload.
7. Do not use git add . or git add -A.
8. Do not commit W3/W4 outputs.
9. If tests fail due script/wrapper issues, debug and fix in this round.

## Existing W1/W2 files to read

Read these first:

- experiments/paper-mascar/workloads/mascar_table_iii_workload_manifest.csv
- experiments/paper-mascar/workloads/mascar_table_iii_workload_manifest_audited.csv
- experiments/paper-mascar/workloads/mascar_table_iii_command_manifest.csv
- experiments/paper-mascar/workloads/wrappers/
- experiments/paper-mascar/workloads/run_available_workload_smoke.sh
- experiments/paper-mascar/workloads/build_available_workloads.sh
- docs/papers/mascar_w1_workload_inventory_report.md
- docs/papers/mascar_w2_workload_command_normalization_report.md

Important W2 caveat:
- The previous actual stencil smoke had result_status=completed_no_explicit_pass and gpgpusim_exit=1.
- W3 collector must classify this separately.
- Do not label completed_no_explicit_pass as correctness pass.

## Directory layout

Create:

- experiments/common/
- experiments/common/gpgpusim_matrix/
- experiments/paper-mascar/workloads/matrix/
- experiments/paper-mascar/workloads/results/

If experiments/common already exists, extend it carefully.

## Common runner

Create:

- experiments/common/gpgpusim_matrix/run_gpgpusim_matrix.sh

Requirements:

- bash script.
- Should be reusable outside Mascar.
- Must not require Python for basic running.
- Must continue after individual run failures.
- Must never delete existing experiment data.
- Must create timestamped output dir by default.

Environment variables:

- CONFIG_MATRIX
- WORKLOAD_MANIFEST
- OUTDIR
- TIMEOUT_SEC default 1200
- MAX_RUNS default 0 meaning no cap
- RUN_PLACEHOLDERS default 1
- RUN_READY default 1
- DRY_RUN_ONLY default 0
- SOURCE_ENV default 1
- GPGPUSIM_ROOT default current repo root
- GPGPU_WORKLOAD_ROOT default /workspace/repos/gpgpu-workloads

Expected config matrix columns:

- config_id
- config_path
- config_role
- enabled
- notes

Expected workload command manifest columns:

- paper_id
- paper_name
- paper_type
- availability_status
- wrapper_path
- wrapper_status
- build_required
- build_command
- run_working_dir
- run_command
- input_size
- timeout_sec
- dry_run_status
- notes

For each config x workload row:

1. Skip config if enabled != 1.
2. If wrapper_path missing, record wrapper_missing and continue.
3. Create run directory:
   OUTDIR/runs/<config_id>/<paper_id>/
4. Copy config files into run dir if config_path contains:
   gpgpusim.config
   config_volta_islip.icnt
5. Set env:
   MASCAR_RUN_DIR=<run dir>
   MASCAR_CONFIG_DIR=<config path>
   MASCAR_TIMEOUT_SEC=<row timeout or TIMEOUT_SEC>
   GPGPUSIM_ROOT=<repo root>
   GPGPU_WORKLOAD_ROOT=<workload root>
6. Run wrapper:
   - If DRY_RUN_ONLY=1, wrapper --dry-run.
   - Else actual wrapper.
7. Use timeout for actual runs.
8. Capture:
   stdout.log
   stderr.log
   combined.log
   exit_code.txt
   command.txt
   env.txt
9. Record a row in run_manifest.csv.

Run manifest columns:

- run_id
- config_id
- config_path
- paper_id
- paper_name
- paper_type
- wrapper_path
- wrapper_status
- run_dir
- timeout_sec
- exit_code
- timeout
- result_status_prelim
- start_iso
- end_iso
- elapsed_sec
- notes

Preliminary status rules:

- exit code 77 -> wrapper_unavailable
- timeout 124 or timeout flag -> timeout
- exit code 0 -> completed_exit0
- nonzero exit with logs -> completed_nonzero_or_error
- missing wrapper -> wrapper_missing
- skipped due MAX_RUNS -> skipped_max_runs
- skipped due disabled config -> skipped_config
- skipped due wrapper_status placeholder and RUN_PLACEHOLDERS=0 -> skipped_placeholder

Do not overinterpret correctness in runner. Collector will classify further.

## Common collector

Create:

- experiments/common/gpgpusim_matrix/collect_gpgpusim_stats.py

Requirements:

- Python 3.
- Input: run directory containing run_manifest.csv.
- Output:
  - results.csv
  - summary.md
  - status_matrix.csv

Parse per-run logs:

- combined.log
- stdout.log
- stderr.log
- stats.txt if present

Extract stats by regex:

Core stats:
- gpu_tot_sim_cycle
- gpu_tot_ipc
- gpu_tot_sim_insn
- gpu_sim_cycle
- gpu_sim_insn

Mascar stats:
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

Cache-ish stats:
- any line containing L1D and hit/miss if easy.
- cacheinst_l1_* if present.
- reservation_fail if present.

Classification rules:

- wrapper_unavailable:
  exit code 77 or log says placeholder/missing.
- timeout:
  timeout flag true.
- crash_assert:
  log contains "Assertion", "assert", "SIGSEGV", "segmentation fault", "Aborted", "core dumped".
- completed_explicit_pass:
  exit code 0 and log contains PASS / Test Passed / passed verification.
- completed_stats_found:
  stats such as gpu_tot_sim_cycle found, even if no explicit PASS.
- completed_no_explicit_pass:
  exit code 0 or nonzero but simulator reached stats, no explicit PASS.
- completed_nonzero_with_stats:
  exit code nonzero but simulator stats found.
- simulator_error_no_stats:
  nonzero exit, no stats.
- no_stats_exit0:
  exit 0, no stats, no pass.
- missing_binary:
  log contains No such file or binary missing.
- missing_data:
  log contains data/input missing.
- unknown:
  fallback.

Important:
- completed_no_explicit_pass and completed_nonzero_with_stats are not correctness pass.
- Record them separately.

Results CSV columns:

- run_id
- config_id
- paper_id
- paper_name
- paper_type
- wrapper_status
- result_status
- exit_code
- timeout
- elapsed_sec
- gpu_tot_sim_cycle
- gpu_tot_ipc
- l1_sat_sample
- l1_sat_saturated
- m2_mp_cycles
- m2_owner_acquire
- m2_nonowner_mem_block
- m3_attempt
- m3_hit
- m3_nack
- m4_enqueue_success
- m4_retry_attempt
- m4_retry_hit
- m4_retry_nack
- m4_retry_requeue
- m4_queue_occupancy_max
- log_path
- notes

Summary MD must include:

1. Total runs.
2. Status breakdown.
3. Breakdown by config.
4. Breakdown by paper_type.
5. Completed runs with stats.
6. Unavailable placeholders.
7. Timeouts.
8. Crashes/asserts.
9. Nonzero-with-stats.
10. M2/M3/M4 activation summary.
11. Recommendations for W4/W5.

## Mascar-specific wrapper around common runner

Create:

- experiments/paper-mascar/workloads/matrix/run_mascar_table_iii_matrix.sh

Purpose:
- Thin wrapper around common runner.
- Defaults:
  CONFIG_MATRIX=experiments/paper-mascar/workloads/matrix/mascar_w4_smoke_config_matrix.csv
  WORKLOAD_MANIFEST=experiments/paper-mascar/workloads/mascar_table_iii_command_manifest.csv
  OUTDIR=experiments/paper-mascar/workloads/results/w4_smoke_<timestamp>
  TIMEOUT_SEC=1200
  RUN_PLACEHOLDERS=1
  RUN_READY=1
  SOURCE_ENV=1

Also create:

- experiments/paper-mascar/workloads/matrix/collect_mascar_table_iii_matrix.py

This may simply call/import the common collector or wrap it.

## W4 smoke config matrix

Create:

- experiments/paper-mascar/workloads/matrix/mascar_w4_smoke_config_matrix.csv

Rows:

1. baseline_off
   config_path=configs/hrl-repro/SM7_QV100_mascar_baseline_off
   enabled=1

2. m4_reexec_load
   config_path=configs/hrl-repro/SM7_QV100_mascar_m4_reexec_load_on
   enabled=1

Optional row disabled by default:
3. m2_owner_sched
4. m3_hitonly_nack

W4 only needs baseline + M4 active by default.

## Documentation

Create:

- docs/papers/mascar_w3_matrix_runner_framework.md

Required sections:

1. Goal
2. Inputs and outputs
3. Common runner
4. Common collector
5. Status classification
6. Mascar-specific wrapper
7. How this supports future papers
8. Known limitations

## Validation

Run:

- bash -n experiments/common/gpgpusim_matrix/run_gpgpusim_matrix.sh
- python3 -m py_compile experiments/common/gpgpusim_matrix/collect_gpgpusim_stats.py
- bash -n experiments/paper-mascar/workloads/matrix/run_mascar_table_iii_matrix.sh
- python3 -m py_compile experiments/paper-mascar/workloads/matrix/collect_mascar_table_iii_matrix.py
- DRY_RUN_ONLY=1 bash experiments/paper-mascar/workloads/matrix/run_mascar_table_iii_matrix.sh
- python3 experiments/common/gpgpusim_matrix/collect_gpgpusim_stats.py <dry-run-outdir>

Do not run actual W4 smoke in W3; W4 guidance will do that.

## W3 output

Create:

- experiments/paper-mascar/workloads/audit/w3_postcheck.md

Include:
- branch and HEAD
- commands run
- dry-run matrix result
- status classification notes
- files changed
- warnings

## Stop conditions

Stop only if:

1. W1/W2 manifest is missing or corrupted.
2. common runner cannot run dry-run.
3. collector cannot parse dry-run output.
4. generated scripts are syntactically invalid and cannot be fixed.
5. repository state becomes unsafe.

Do not stop because wrappers are placeholders.
