# Mascar W25F Guidance: Energy Rerun Plan After W25E Recovery

## Stage position

This is W25F.

W25 originally ran the integrated energy matrix but collected true power fields = 0.
W25E diagnosed and fixed the root cause:
- common matrix runner did not copy AccelWattch power artifacts from workload CWD / exec dir back to run_dir
- collector scanned too narrow a log scope
- W25E fixed runner artifact recovery and bounded recursive collector scan

W25F must rerun the W25 integrated energy sweep with the fixed runner and collector, then produce a valid current-simulator power / derived-energy trend report.

## Goal

Rerun the W25 integrated energy matrix:

- workloads:
  - bp_2
  - srad_1
  - bp_1
  - spmv
  - mri_q
  - pathfinder

- configs:
  - energy_baseline_off
  - energy_m4_reexec_load

Expected actual run rows:
- 6 workloads x 2 configs = 12 rows

The success condition is:
- all or most rows complete
- power fields are recovered for rows that produce AccelWattch reports
- derived energy ratio can be computed for baseline vs M4 where both sides have valid power/cycles
- report clearly states this is current-simulator energy trend, not paper GPUWattch/GTX480 reproduction

## Hard constraints

1. Do not create a new branch.
2. Do not fetch upstream.
3. Do not modify Mascar M1-M4 mechanism behavior.
4. Do not modify W25E runner/collector fixes unless a bug is found.
5. Preserve W15 m3diag parsing.
6. Preserve W16/W25E power parsing.
7. Do not run full benchmark suites.
8. Run only the W25F selected 6 workloads x 2 configs by default.
9. Every actual run must use timeout.
10. Raw logs must be archived under /workspace/tmp and not committed.
11. Do not use git add . or git add -A.
12. Do not commit W25F outputs.
13. Use start_ts/end_ts in postcheck.
14. Report must not claim paper 12% energy saving reproduction.

## Inputs to read

Read:

- docs/papers/mascar_w25e_energy_field_recovery_report.md
- experiments/paper-mascar/energy/W25E/w25e_final_diagnosis.csv
- experiments/common/gpgpusim_matrix/run_gpgpusim_matrix.sh
- experiments/common/gpgpusim_matrix/collect_gpgpusim_stats.py

Read W25 inputs:

- experiments/paper-mascar/energy/W25/matrix/w25_energy_config_matrix.csv
- experiments/paper-mascar/energy/W25/matrix/w25_energy_workload_manifest.csv
- experiments/paper-mascar/energy/W25/matrix/w25_energy_run_plan.csv
- docs/papers/mascar_w25a_energy_integrated_run_plan.md
- docs/papers/mascar_w25_integrated_energy_sweep_report.md

Read energy configs:

- configs/hrl-repro/SM7_QV100_mascar_energy_baseline_off/
- configs/hrl-repro/SM7_QV100_mascar_energy_m4_reexec_load_on/

If W25 inputs are missing:
- reconstruct from W25 docs and known selected workloads
- mark fallback_mode=1 in report

## Required directories

Create:

- experiments/paper-mascar/energy/W25F/
- experiments/paper-mascar/energy/W25F/matrix/
- experiments/paper-mascar/energy/W25F/results/
- experiments/paper-mascar/energy/W25F/audit/

## W25F plan script

Create:

- experiments/paper-mascar/energy/W25F/matrix/prepare_w25f_energy_rerun_plan.py

Responsibilities:

1. Read W25 workload/config manifest if available.
2. Recreate 6-workload selection:
   - bp_2
   - srad_1
   - bp_1
   - spmv
   - mri_q
   - pathfinder
3. Confirm wrappers are ready.
4. Confirm energy configs exist.
5. Confirm each energy config has AccelWattch/power knobs and XML reference.
6. Confirm W25E fixed runner contains power_artifacts recovery logic.
7. Confirm collector contains recursive scan / power field parsing.
8. Generate W25F config matrix, workload manifest, and run plan.

Outputs:

- experiments/paper-mascar/energy/W25F/matrix/w25f_energy_config_matrix.csv
- experiments/paper-mascar/energy/W25F/matrix/w25f_energy_workload_manifest.csv
- experiments/paper-mascar/energy/W25F/matrix/w25f_energy_run_plan.csv
- experiments/paper-mascar/energy/W25F/matrix/w25f_preflight_check.csv
- docs/papers/mascar_w25f_energy_rerun_plan.md
- experiments/paper-mascar/energy/W25F/audit/w25f_a_postcheck.md

## Preflight checks

w25f_preflight_check.csv columns:

- check_id
- item
- status
- evidence
- notes

Required checks:

- runner_has_power_artifact_recovery
- collector_has_recursive_scan
- collector_has_kernel_avg_power_pattern
- collector_has_gpu_tot_avg_power_pattern
- energy_baseline_config_exists
- energy_m4_config_exists
- accelwattch_xml_exists_baseline
- accelwattch_xml_exists_m4
- workload_manifest_rows
- run_plan_rows

## Config matrix

w25f_energy_config_matrix.csv rows:

1. energy_baseline_off
   config_path=configs/hrl-repro/SM7_QV100_mascar_energy_baseline_off
   config_role=energy_baseline
   enabled=1

2. energy_m4_reexec_load
   config_path=configs/hrl-repro/SM7_QV100_mascar_energy_m4_reexec_load_on
   config_role=energy_m4
   enabled=1

## Workload manifest columns

- workload_id
- paper_id
- paper_name
- paper_type
- wrapper_path
- wrapper_status
- selected_for_w25f
- selection_reason
- timeout_sec
- prior_w25_status
- notes

## Run plan columns

- run_id
- config_id
- config_path
- config_role
- workload_id
- paper_id
- wrapper_path
- selected_for_run
- timeout_sec
- expected_power_artifacts
- notes

## Report requirements

docs/papers/mascar_w25f_energy_rerun_plan.md sections:

1. Goal
2. W25 issue summary
3. W25E fix summary
4. Selected workloads
5. Config matrix
6. Preflight checks
7. Run plan
8. Expected outputs
9. Caveats

## Validation

Run:

- python3 -m py_compile experiments/paper-mascar/energy/W25F/matrix/prepare_w25f_energy_rerun_plan.py
- python3 experiments/paper-mascar/energy/W25F/matrix/prepare_w25f_energy_rerun_plan.py
- Check run plan has 12 rows.
- Check preflight checks pass or have clear warnings.
- git diff --check

## Stop conditions

Stop only if:
1. energy configs are missing and cannot be reconstructed
2. no selected wrappers exist
3. planner cannot generate run plan
4. W25E runner/collector fixes are absent

Do not stop because some preflight checks warn. Document and continue to W25F-B if actual run is still possible.
