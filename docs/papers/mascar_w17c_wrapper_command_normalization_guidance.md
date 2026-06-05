# Mascar W17C Guidance: Wrapper and Command Normalization

## Stage position

This is W17C.

W17A audited candidates. W17B attempted builds/recovered binaries. W17C updates wrappers and command manifests so newly available workloads can be dry-run and smoked.

## Goal

Convert built/recovered workload candidates into consistent Table III wrappers and command manifest updates.

## Hard constraints

1. Do not modify M1-M4 Mascar mechanism code.
2. Do not treat app-level wrapper as exact phase mapping.
3. Do not remove existing working wrappers.
4. Do not fabricate commands for missing binaries/data.
5. All wrappers must support --help, --dry-run, --print-command.
6. Actual run of unavailable wrapper must exit 77.
7. Do not run full benchmark suites.
8. Every changed wrapper must pass bash -n.
9. Keep all 30 Table III rows represented.

## Existing wrapper convention

Read existing wrappers under:

- experiments/paper-mascar/workloads/wrappers/

Preserve behavior:
- MASCAR_RUN_DIR
- MASCAR_CONFIG_DIR
- MASCAR_TIMEOUT_SEC
- GPGPUSIM_ROOT
- GPGPU_WORKLOAD_ROOT
- --dry-run
- --print-command
- --help
- exit 77 for unavailable

Also inspect:

- experiments/paper-mascar/workloads/wrappers/_run_table_iii_common.sh

W8 fixed config override behavior here. Do not regress that.

## Wrapper update policy

For each workload newly built or recovered:

1. Add or update `run_<paper_id>.sh`.
2. Use direct command if binary supports direct run.
3. Use data path from W17B if available.
4. If phase is not exact, mark in manifest:
   - app_level_pending_kernel_trace
5. Do not claim exact phase mapping until W18 kernel trace.

For rows sharing same app:

- BP-1/BP-2 may use same app-level wrapper until W18.
- histo-1/2/3 may use same app-level wrapper until W18.
- kmeans-1/2 may use same app-level wrapper until W18.
- leuko-1/2/3 may use same app-level wrapper until W18.
- mrig-1/2/3 may use same app-level wrapper until W18.
- sad-1/2 may use same app-level wrapper until W18.
- srad-1/2 may use same app-level wrapper until W18.

But each paper row must remain separate in command manifest.

## Command manifest update

Create a W17 updated command manifest, do not overwrite original until reviewed:

- experiments/paper-mascar/workloads/matrix/W17/w17_command_manifest_updated.csv

Columns should follow W2 command manifest and include:

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
- phase_mapping_status
- W17_update_status
- notes

W17_update_status values:

- unchanged_ready
- newly_ready
- wrapper_fixed
- app_level_ready_phase_pending
- still_missing_binary
- still_missing_source
- still_missing_data
- unsupported_env
- unknown

## Wrapper smoke helper

Create:

- experiments/paper-mascar/workloads/matrix/W17/w17c_smoke_new_wrappers.sh

Requirements:

- bash
- dry-run all 30 wrappers
- actual smoke newly_ready wrappers only by default
- env vars:
  - W17_RUN_ACTUAL default 1
  - W17_MAX_ACTUAL_RUNS default 10
  - W17_TIMEOUT_SEC default 1200
  - W17_ONLY_PAPER_ID optional
  - W17_OUTDIR default experiments/paper-mascar/workloads/results/W17/w17c_smoke_<timestamp>
- use baseline_off config by default
- continue after failure
- write:
  - w17c_smoke_results.csv
  - logs

## Required outputs

Create:

- experiments/paper-mascar/workloads/matrix/W17/w17_command_manifest_updated.csv
- experiments/paper-mascar/workloads/matrix/W17/w17c_smoke_new_wrappers.sh
- experiments/paper-mascar/workloads/results/W17/w17c_smoke_results.csv
- docs/papers/mascar_w17c_wrapper_command_normalization.md
- experiments/paper-mascar/workloads/audit/W17/w17c_postcheck.md
- updated/new wrappers under experiments/paper-mascar/workloads/wrappers/

## Report requirements

docs/papers/mascar_w17c_wrapper_command_normalization.md must include:

1. Goal
2. Wrapper updates made
3. Newly ready workloads
4. App-level phase pending workloads
5. Still unavailable workloads
6. Smoke results
7. W18 phase tracing dependencies
8. Risks and limitations

## Validation

Run:

- bash -n every changed wrapper
- bash -n experiments/paper-mascar/workloads/matrix/W17/w17c_smoke_new_wrappers.sh
- dry-run smoke all 30 wrappers
- actual smoke newly_ready wrappers if feasible
- Check updated command manifest has 30 rows

## Stop conditions

Stop only if:
1. existing ready wrappers are broken and cannot be restored.
2. updated manifest loses rows.
3. wrapper smoke helper fails globally.
4. repository state becomes unsafe.

Do not stop because individual wrappers remain unavailable.
