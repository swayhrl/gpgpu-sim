# Mascar W25A Guidance: Integrated Energy Sweep Plan

## Stage position

This is W25A of the Mascar current-simulator energy integration effort.

Current state expected before W25:
- W16 built current-simulator power/derived-energy pipeline.
- W24 Table III refreshed sweep has completed and should provide refreshed ready-row results.
- W25 integrates energy analysis with the current Table III / activation / full-suite framework.
- W25 does not attempt paper-exact GPUWattch GTX480 reproduction.

W25A prepares the run plan. W25B executes the energy sweep. W25C analyzes. W25D closes out.

## Goal

Create an energy sweep plan that compares current-simulator energy/power trend between:

- energy_baseline_off
- energy_m4_reexec_load

for selected stable workloads.

The purpose is to validate a reusable energy pipeline for future papers and user experiments, not to claim paper-equivalent 12% energy saving.

## Paper context

The Mascar paper reports energy using GPUWattch on a GPGPU-Sim v3.2.2 GTX480/Fermi model. Current repository uses a different simulator/config environment. Therefore W25 must label all energy as current-simulator trend.

## Hard constraints

1. Do not create a new branch.
2. Do not fetch upstream.
3. Do not modify Mascar M1-M4 mechanism behavior.
4. Do not overwrite W16 collector power parsing.
5. Preserve W15 m3diag parsing and W16 power parsing in the collector.
6. Do not claim paper GPUWattch/GTX480 energy equivalence.
7. Do not claim reproduction of paper's 12% energy saving.
8. Do not run full benchmark suites in W25.
9. Do not actual-run unavailable workloads.
10. Every actual run must use timeout.
11. Raw logs must be archived under /workspace/tmp and not committed.
12. Do not use git add . or git add -A.
13. Do not commit W25/W26 outputs.
14. Use start_ts/end_ts in postcheck.

## Inputs to read

Read if present:

- experiments/paper-mascar/workloads/results/W24/w24_tableiii_refreshed_results.csv
- experiments/paper-mascar/workloads/results/W24/w24_tableiii_coverage_manifest.csv
- experiments/paper-mascar/workloads/results/W24/w24_tableiii_activation_matrix.csv
- experiments/paper-mascar/workloads/results/W24/w24_tableiii_speedup_summary.csv
- experiments/paper-mascar/workloads/results/W24/w24_tableiii_geomean_summary.csv
- docs/papers/mascar_w24_tableiii_refreshed_sweep_report.md
- experiments/paper-mascar/energy/W16C/w16_energy_latest_results.csv
- experiments/paper-mascar/energy/W16D/w16_energy_trend_summary.csv
- experiments/paper-mascar/energy/W16D/w16_energy_ratio_by_workload.csv
- docs/papers/mascar_w16_energy_trend_report.md
- experiments/suites/common/full_suite_ready_manifest.csv
- experiments/suites/results/W23/w23_baseline_results.csv if present

If W24 outputs are absent:
- do not fail immediately.
- fallback to W16/W23/W8 known stable workloads.
- mark fallback_mode=1 in W25A report.

## Required directories

Create:

- experiments/paper-mascar/energy/W25/
- experiments/paper-mascar/energy/W25/matrix/
- experiments/paper-mascar/energy/W25/results/
- experiments/paper-mascar/energy/W25/audit/

## Workload selection policy

Select a bounded workload set.

Priority 1: W24 ready rows with completed baseline and m4 results, especially memory-intensive or Mascar-active rows.

Priority 2: W16 stable energy workloads:
- spmv
- mri_q
- pathfinder

Priority 3: W11/W12/W24 active rows if available:
- bp_2
- srad_1
- srad_2
- bp_1

Priority 4: current smoke-ready suite rows only if small.

Default maximum:
- 6 workloads
- 2 configs
- total actual runs <= 12 by default

Do not include placeholders/unavailable rows.

## Config matrix

Create:

- experiments/paper-mascar/energy/W25/matrix/w25_energy_config_matrix.csv

Rows:

1. energy_baseline_off
   config_path=configs/hrl-repro/SM7_QV100_mascar_energy_baseline_off
   config_role=energy_baseline
   enabled=1

2. energy_m4_reexec_load
   config_path=configs/hrl-repro/SM7_QV100_mascar_energy_m4_reexec_load_on
   config_role=energy_m4
   enabled=1

Optional disabled rows:
3. baseline_off enabled=0
4. m4_reexec_load enabled=0

If energy configs are missing:
- create config matrix with enabled=0
- write W25 energy unavailable reason
- do not fabricate configs

## Run-plan script

Create:

- experiments/paper-mascar/energy/W25/matrix/prepare_w25_energy_run_plan.py

Responsibilities:

1. Read W24/W16/W23 inputs.
2. Decide selected workloads.
3. Confirm wrapper paths and readiness.
4. Create W25 workload manifest.
5. Create run plan.
6. Mark paper_type and activation history.
7. Mark correctness prior status.

Outputs:

- experiments/paper-mascar/energy/W25/matrix/w25_energy_workload_manifest.csv
- experiments/paper-mascar/energy/W25/matrix/w25_energy_run_plan.csv
- experiments/paper-mascar/energy/W25/matrix/w25_energy_selection_summary.csv
- experiments/paper-mascar/energy/W25/audit/w25a_input_status.csv

## w25_energy_workload_manifest.csv columns

- workload_id
- paper_id
- paper_name
- paper_type
- suite_id
- wrapper_path
- wrapper_status
- selected_for_w25
- selection_reason
- prior_correctness_status
- prior_m2_active
- prior_m3_active
- prior_m4_active
- timeout_sec
- notes

## w25_energy_run_plan.csv columns

- run_id
- config_id
- config_path
- config_role
- workload_id
- paper_id
- wrapper_path
- selected_for_run
- timeout_sec
- expected_energy_fields
- notes

## W25A report

Create:

- docs/papers/mascar_w25a_energy_integrated_run_plan.md

Required sections:

1. Goal
2. Paper energy caveat
3. W16 energy pipeline status
4. W24 inputs used
5. Selected workloads
6. Config matrix
7. Run row count
8. Fallback mode, if any
9. Risks and assumptions
10. W25B execution plan

## W25A postcheck

Create:

- experiments/paper-mascar/energy/W25/audit/w25a_postcheck.md

Include:
- start_iso
- end_iso
- elapsed_sec
- branch and HEAD
- input files found/missing
- selected workload count
- enabled config count
- run plan row count
- warnings

## Validation

Run:

- python3 -m py_compile experiments/paper-mascar/energy/W25/matrix/prepare_w25_energy_run_plan.py
- python3 experiments/paper-mascar/energy/W25/matrix/prepare_w25_energy_run_plan.py
- Check workload manifest exists.
- Check run plan exists.
- Check run plan has rows if energy configs exist.
- Check unavailable reason exists if configs missing.
- git diff --check

## Stop conditions

Stop W25A only if:
1. no manifest input can be read
2. planner loses rows or creates malformed CSV
3. energy configs are missing and unavailable report cannot be generated

Do not stop because W24 is absent. Fallback to W16/W23 stable workloads and document.
