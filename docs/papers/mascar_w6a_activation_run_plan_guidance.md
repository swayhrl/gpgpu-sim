# Mascar W6A Guidance: Activation-Aware Run Plan

## Stage position

This is W6A of the Mascar Table III workload coverage effort.

Completed before W6:
- M1-M4 implemented Mascar mechanisms.
- M5/M6 mechanism closeout ran a focused hotspot validation.
- W1/W2 created Table III workload inventory and wrappers.
- W3/W4 created matrix runner/collector and ran baseline + M4 smoke.
- W5 screened whether existing smoke inputs activate M2/M3/M4 counters.
- W5C should provide activation-ready input/workload candidates.

W6A must integrate W5C output, build a concrete run plan, and prepare full-sweep inputs. W6B will execute that plan. W6C will summarize trends.

## Paper target

The paper evaluates Rodinia/Parboil Table III workloads and interprets results by memory-intensive vs compute-intensive classes. Current simulator/config differs from the paper, so W6 should check trends and mechanism activation, not claim numerical equivalence.

## Hard constraints

1. Do not modify Mascar M1-M4 mechanism code unless W6B finds a real bug later.
2. Do not fetch upstream.
3. Do not create a new branch.
4. Do not run full benchmark suites in W6A.
5. Do not fabricate missing workloads or inputs.
6. Do not treat completed_no_explicit_pass as correctness pass.
7. Keep all missing/placeholder Table III rows represented in manifests.
8. Do not use git add . or git add -A.
9. Do not commit W6 outputs.

## Required inputs to read

Read:

- experiments/paper-mascar/workloads/mascar_table_iii_workload_manifest.csv
- experiments/paper-mascar/workloads/mascar_table_iii_command_manifest.csv
- experiments/paper-mascar/workloads/matrix/W5A/activation_counters.csv
- experiments/paper-mascar/workloads/matrix/W5B/activation_matrix.csv
- experiments/paper-mascar/workloads/matrix/W5B/subset_manifest.csv
- experiments/paper-mascar/workloads/matrix/W5C/activation_ready_subset_manifest.csv if present
- experiments/paper-mascar/workloads/matrix/W5C/w5c_postcheck.md if present
- experiments/paper-mascar/workloads/results/w4_smoke_latest_results.csv
- docs/papers/mascar_w5b_activation_matrix_report.md
- docs/papers/mascar_w5c_activation_input_search_report.md if present

If W5C files are absent:
- do not fail immediately.
- generate W6 run plan from W4/W5 ready wrappers, but mark activation input unavailable.
- W6B should still run all ready wrappers if practical.

## Required directories

Create:

- experiments/paper-mascar/workloads/matrix/W6/
- experiments/paper-mascar/workloads/results/W6/
- experiments/paper-mascar/workloads/audit/W6/

## W6 config matrix

Create:

- experiments/paper-mascar/workloads/matrix/W6/w6_config_matrix.csv

Rows:

1. baseline_off
   config_path=configs/hrl-repro/SM7_QV100_mascar_baseline_off
   config_role=baseline
   enabled=1

2. m2_owner_sched
   config_path=configs/hrl-repro/SM7_QV100_mascar_m2_owner_sched_on
   config_role=scheduling_only
   enabled=1

3. m3_hitonly_nack
   config_path=configs/hrl-repro/SM7_QV100_mascar_m3_hitonly_nack_on
   config_role=scheduling_hitonly
   enabled=1

4. m4_reexec_load
   config_path=configs/hrl-repro/SM7_QV100_mascar_m4_reexec_load_on
   config_role=scheduling_hitonly_reexec
   enabled=1

Optional disabled rows:
5. m1_l1sat_probe enabled=0
6. m4_reexec_probe_only enabled=0
7. old_proxy_mascar enabled=0 if config exists

Do not enable old proxy scheduling by default.

## W6 workload plan generator

Create:

- experiments/paper-mascar/workloads/matrix/W6/prepare_w6_run_plan.py

Purpose:
- Read W1/W2/W5/W5C inputs.
- Generate W6 workload manifests and run plan.

Outputs:

1. experiments/paper-mascar/workloads/matrix/W6/w6_workload_manifest_all_tableiii.csv
   - all 30 rows
   - includes placeholder/unavailable rows

2. experiments/paper-mascar/workloads/matrix/W6/w6_workload_manifest_ready.csv
   - wrappers with wrapper_status=ready

3. experiments/paper-mascar/workloads/matrix/W6/w6_workload_manifest_activation_ready.csv
   - W5C active subset if available
   - otherwise empty with headers

4. experiments/paper-mascar/workloads/matrix/W6/w6_run_plan.csv
   - selected workload x selected configs
   - should prioritize activation_ready rows first
   - then all ready rows
   - placeholders listed with run_status=skip_unavailable unless RUN_PLACEHOLDERS is requested

Required columns for workload manifests:

- paper_id
- paper_name
- paper_type
- paper_suite
- paper_inst_per_l1_miss
- wrapper_path
- wrapper_status
- availability_status
- phase_mapping_status
- w5_m2_active
- w5_m3_active
- w5_m4_active
- w5c_m2_triggered
- w5c_m3_triggered
- w5c_m4_triggered
- selected_for_w6
- selection_reason
- timeout_sec
- notes

Required columns for run plan:

- run_group
- config_id
- config_path
- paper_id
- paper_name
- paper_type
- wrapper_path
- wrapper_status
- selected_for_run
- run_priority
- timeout_sec
- expected_activation
- notes

Selection rules:

1. Include all W5C activation-ready rows if present.
2. Include all ready wrappers from W2/W4, even if W5 counters were zero, because W6 still needs ready coverage.
3. Include placeholders in all_tableiii manifest but do not run them by default.
4. Mark memory-intensive workloads as priority if ready.
5. Keep compute-intensive workloads for safety/regression trend.
6. If a workload lacks explicit correctness PASS, keep it but mark correctness_status=not_proven.

## Documentation

Create:

- docs/papers/mascar_w6a_activation_run_plan.md

Required sections:

1. Goal
2. Inputs from W5/W5C
3. Config matrix
4. Workload selection policy
5. Activation-ready subset
6. Ready wrappers
7. Unavailable Table III rows
8. Run plan summary
9. Risks and assumptions
10. How W6B should execute

## Validation

Run:

- python3 -m py_compile experiments/paper-mascar/workloads/matrix/W6/prepare_w6_run_plan.py
- python3 experiments/paper-mascar/workloads/matrix/W6/prepare_w6_run_plan.py
- Check all_tableiii manifest has 30 rows.
- Check config matrix has enabled baseline/m2/m3/m4 rows.
- Check run_plan has rows for all ready wrappers x enabled configs.
- Check activation_ready manifest exists even if empty.

## W6A postcheck

Create:

- experiments/paper-mascar/workloads/audit/W6/w6a_postcheck.md

Include:
- branch and HEAD
- W5C input status
- row counts
- selected workloads
- config rows
- warnings
