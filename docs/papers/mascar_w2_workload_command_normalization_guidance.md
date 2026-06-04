# Mascar W2 Detailed Guidance: Workload Build and Command Normalization

## Stage position

This is W2 of the workload coverage effort.

W1 creates and audits the canonical Table III workload inventory.
W2 turns available or partially available workloads into normalized wrappers and command manifests.

W2 must not run a full Mascar experiment matrix.
W2 may run lightweight dry-run or help/build checks.
W2 may build available workloads if build commands are obvious and bounded by timeout.
W2 must not fabricate commands for missing workloads.

## Goals

1. Normalize workload commands for all Table III rows.
2. Create wrappers for available workloads.
3. Create placeholder wrappers for unavailable workloads that fail clearly with status 77.
4. Create build helper for available workloads.
5. Upgrade the existing M5 runner compatibility so future W3/W4 can run Table III workloads.
6. Preserve phase distinction even when local benchmark only provides app-level command.

## Required outputs

Create or update:

- experiments/paper-mascar/workloads/mascar_table_iii_command_manifest.csv
- experiments/paper-mascar/workloads/build_available_workloads.sh
- experiments/paper-mascar/workloads/run_available_workload_smoke.sh
- experiments/paper-mascar/workloads/wrappers/*.sh
- docs/papers/mascar_w2_workload_command_normalization_report.md
- experiments/paper-mascar/workloads/audit/w2_postcheck.md
- experiments/paper-mascar/workloads/audit/w1_w2_review_manifest.csv

## Command manifest

Create:

- experiments/paper-mascar/workloads/mascar_table_iii_command_manifest.csv

Required columns:

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

wrapper_status values:

- ready
- placeholder_missing_binary
- placeholder_missing_data
- placeholder_missing_source
- placeholder_phase_unknown
- unsupported_env
- unknown

Default timeout:

- tiny: 300
- short: 1200
- medium: 3600
- long: 7200
- unknown: 1200

For W2, prefer conservative short/tiny inputs if available.

## Wrapper convention

Create one wrapper per Table III paper row:

- experiments/paper-mascar/workloads/wrappers/run_<paper_id>.sh

Use sanitized paper_id examples:

- run_bp_1.sh
- run_bp_2.sh
- run_bfs.sh
- run_histo_1.sh
- run_histo_2.sh
- run_histo_3.sh
- run_histogram.sh
- run_kmeans_1.sh
- run_kmeans_2.sh
- run_lavamd.sh
- run_lbm.sh
- run_leuko_1.sh
- run_leuko_2.sh
- run_leuko_3.sh
- run_mri_q.sh
- run_mrig_1.sh
- run_mrig_2.sh
- run_mrig_3.sh
- run_mummer.sh
- run_particle.sh
- run_pathfinder.sh
- run_sad_1.sh
- run_sad_2.sh
- run_sgemm.sh
- run_spmv.sh
- run_srad_1.sh
- run_srad_2.sh
- run_stencil.sh
- run_tpacf.sh
- run_cutcp.sh

Every wrapper must:

- be executable
- use bash
- support --dry-run
- support --print-command
- support --help
- accept environment variables:
  - MASCAR_RUN_DIR
  - MASCAR_CONFIG_DIR
  - MASCAR_TIMEOUT_SEC
  - GPGPUSIM_ROOT
  - GPGPU_WORKLOAD_ROOT
- never write large logs outside MASCAR_RUN_DIR
- exit 77 for unavailable/missing workload
- exit 0 for --dry-run if the wrapper syntax is valid, even if workload missing, but print status
- run the actual workload only when invoked without --dry-run/--print-command

For available workloads, wrapper should:
- cd to working directory or prepared run dir.
- copy or symlink required input files only if needed.
- invoke binary with local command.
- not modify source files.

For missing workloads, placeholder wrapper should:
- print clear missing reason.
- print paper workload identity.
- print recommended action.
- exit 77 for actual run.

## Reusing existing M5 infrastructure

Read existing files:

- experiments/paper-mascar/m5_workload_manifest.csv
- experiments/paper-mascar/run_m5_focused_validation.sh
- experiments/paper-mascar/collect_m5_results.py
- experiments/paper-mascar/m5a_runtime_env_audit.md
- experiments/paper-mascar/m5_results_summary.md

The current focused validation found rodinia_hotspot. Use that evidence to make a ready wrapper for any matching hotspot/histo/stencil path only if it truly maps to a Table III row. Do not invent a Table III row for hotspot because hotspot is not in Table III.

Important:
- hotspot was useful for sanity but is not part of Mascar Table III.
- Keep hotspot in a separate "extra_smoke" manifest if needed, not as one of the 30 canonical rows.

If useful, create:

- experiments/paper-mascar/workloads/extra_smoke_workloads.csv

with rodinia_hotspot as extra smoke only.

## Build helper

Create:

- experiments/paper-mascar/workloads/build_available_workloads.sh

Requirements:

- bash
- no full rebuild unless requested
- env vars:
  - WORKLOAD_MANIFEST
  - BUILD_TIMEOUT_SEC default 1200
  - MAX_BUILDS default 10
  - DRY_RUN default 0
- read command manifest
- build only rows with build_required yes and known build_command
- use timeout
- write logs to:
  - experiments/paper-mascar/workloads/build_logs/YYYYMMDD_HHMMSS/
- continue after failures
- write:
  - build_results.csv

If no build commands are known:
- print that no buildable workloads were found.
- exit 0.

## Smoke helper

Create:

- experiments/paper-mascar/workloads/run_available_workload_smoke.sh

Purpose:
- run wrapper --dry-run for all 30.
- optionally run actual command for a small number of ready wrappers.

Requirements:

- env vars:
  - COMMAND_MANIFEST
  - SMOKE_OUTDIR
  - RUN_ACTUAL default 0
  - MAX_ACTUAL_RUNS default 3
  - TIMEOUT_SEC default 1200
- always run wrapper --dry-run for all wrappers.
- actual run only for ready wrappers when RUN_ACTUAL=1.
- save logs under SMOKE_OUTDIR.
- write:
  - smoke_results.csv

## Minimal actual runs

W2 should not run full simulations.

Allowed actual runs:
- wrapper --dry-run for all 30.
- actual run for at most 1 to 3 ready wrappers, only if already proven short and self-contained.
- use timeout.
- if actual run fails due wrapper bug, fix it.
- if actual run fails due missing workload data/env, mark unavailable; do not force.

## Reports

Create:

- docs/papers/mascar_w2_workload_command_normalization_report.md

Required sections:

1. Goal
2. Inputs from W1
3. Wrapper convention
4. Available ready workloads
5. Placeholder workloads
6. Build helper
7. Smoke helper
8. Extra smoke workloads
9. How W3/W4 should use this infrastructure
10. Remaining gaps to full Table III coverage

Create:

- experiments/paper-mascar/workloads/audit/w2_postcheck.md

Include:

- start_iso
- end_iso
- elapsed_sec
- branch and HEAD
- git status before and after
- files changed
- number of canonical workload rows
- number of wrappers created
- number ready
- number placeholders by reason
- commands run
- dry-run result
- actual smoke result if any
- review pack path
- warnings

## Review pack

Create:

- /workspace/tmp/mascar_w1_w2_review_pack_YYYYMMDD_HHMMSS.tar.gz

Include:

- docs/papers/mascar_w1_workload_inventory_report.md
- docs/papers/mascar_w2_workload_command_normalization_report.md
- experiments/paper-mascar/workloads/
- full git diff patch
- postcheck files

Do not commit W1/W2 outputs.

## Validation commands

Run:

- git diff --check
- python3 -m py_compile experiments/paper-mascar/workloads/scripts/audit_table_iii_workloads.py
- bash -n experiments/paper-mascar/workloads/build_available_workloads.sh
- bash -n experiments/paper-mascar/workloads/run_available_workload_smoke.sh
- for each wrapper: bash -n wrapper
- run dry-run smoke:
  bash experiments/paper-mascar/workloads/run_available_workload_smoke.sh

If actual smoke is safe and obvious:
  RUN_ACTUAL=1 MAX_ACTUAL_RUNS=1 bash experiments/paper-mascar/workloads/run_available_workload_smoke.sh

Do not run full benchmark suites.

## Stop conditions

Stop only if:

1. wrapper generation loses canonical workload rows.
2. generated wrappers are syntactically invalid and cannot be fixed.
3. audit script fails fundamentally.
4. repository state becomes unsafe.
5. elapsed time exceeds 110 minutes.

Do not stop because many workloads are unavailable.
