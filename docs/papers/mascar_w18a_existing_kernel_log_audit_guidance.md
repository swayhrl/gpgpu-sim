# Mascar W18A Guidance: Existing Kernel Launch Log Audit

## Stage position

This is W18A of the Mascar Table III phase mapping effort.

Current state:
- W17 expanded Table III ready rows from 6 to 15.
- W17 marked several rows as app_level_pending_kernel_trace.
- W18 must determine kernel launch name/order for these app-level rows and build a paper-row to local-launch mapping.

W18A is audit-first:
- inspect existing logs
- inspect source code for existing kernel launch prints
- build parser scaffolding
- do not modify simulator source yet unless W18A proves existing logs are insufficient and W18B starts.

## Primary target rows

W18 primary targets are W17 phase_pending rows:

- bp_1
- bp_2
- histo_1
- histo_2
- histo_3
- kmeans_1
- kmeans_2
- srad_1
- srad_2

Secondary:
- all 15 ready rows can be traced if runtime permits.

## Why this matters

The Mascar paper Table III rows are kernel/phase-level entries. A local wrapper may run an entire app, while the paper row may refer to one kernel launch in that app. For example, BP-1 and BP-2 are not necessarily two separate binaries; they may be different kernel launches inside one backprop app run.

## Hard constraints

1. Do not create a new branch.
2. Do not fetch upstream.
3. Do not modify M1-M4 Mascar mechanisms in W18A.
4. Do not run full benchmark suites in W18A.
5. Do not overwrite canonical manifests until W18 closeout review.
6. Do not claim exact phase mapping without launch evidence.
7. Keep all 30 Table III rows represented.
8. Do not use git add . or git add -A.
9. Do not commit W18 outputs.
10. Raw logs must be archived to /workspace/tmp if large.

## Existing files to read

Read:

- experiments/paper-mascar/workloads/mascar_table_iii_workload_manifest.csv
- experiments/paper-mascar/workloads/mascar_table_iii_command_manifest.csv
- experiments/paper-mascar/workloads/matrix/W17/w17_phase_pending_manifest.csv
- experiments/paper-mascar/workloads/matrix/W17/w17_ready_manifest.csv
- experiments/paper-mascar/workloads/matrix/W17/w17_next_action_for_W18.csv
- docs/papers/mascar_w17_workload_availability_expansion_report.md
- existing run logs under experiments/paper-mascar/workloads/results/ if small enough to inspect

## Source search

Search for existing kernel launch names/order prints.

Use commands like:

grep -RIn "kernel_info_t\|kernel.*name\|get_name\|name()\|grid_dim\|block_dim\|launch\|stream_manager\|print.*kernel" src/gpgpu-sim src/cuda-sim src 2>/dev/null | head -n 300

Also inspect:
- src/gpgpu-sim/gpu-sim.cc
- src/gpgpu-sim/gpu-sim.h
- src/cuda-sim/
- any stream_manager related code

Do not assume exact function names. Search and inspect.

## Log search

Search existing run logs for kernel launch evidence:

- kernel
- launch
- grid
- block
- function
- paperrepro_kernel
- gpu_sim_cycle
- cudaLaunch

Create concise grep output; do not dump huge logs.

## Parser scaffolding

Create:

- experiments/common/gpgpusim_matrix/collect_kernel_trace.py

Initial behavior:
- input: run output directory or one log file
- parse existing trace patterns if any
- parse future W18B paperrepro_kernel_* lines
- output kernel_trace.csv

Required CSV columns:

- run_id
- config_id
- paper_id
- paper_name
- app
- wrapper_path
- trace_source
- launch_index
- kernel_uid
- kernel_name
- grid_x
- grid_y
- grid_z
- block_x
- block_y
- block_z
- stream_id
- begin_cycle
- end_cycle
- notes

If fields are unavailable, leave blank.

The parser must not crash if no trace lines are found.

## W18A outputs

Create directories:

- experiments/paper-mascar/workloads/matrix/W18/
- experiments/paper-mascar/workloads/results/W18/
- experiments/paper-mascar/workloads/audit/W18/

Create:

- experiments/common/gpgpusim_matrix/collect_kernel_trace.py
- experiments/paper-mascar/workloads/matrix/W18/w18a_existing_log_trace_audit.py
- experiments/paper-mascar/workloads/matrix/W18/w18a_phase_pending_targets.csv
- experiments/paper-mascar/workloads/audit/W18/w18a_kernel_source_grep.txt
- experiments/paper-mascar/workloads/audit/W18/w18a_existing_log_grep.txt
- docs/papers/mascar_w18a_existing_kernel_log_audit.md
- experiments/paper-mascar/workloads/audit/W18/w18a_postcheck.md

## w18a_phase_pending_targets.csv columns

- paper_id
- paper_name
- paper_type
- paper_app
- wrapper_path
- current_phase_mapping_status
- trace_priority
- notes

## W18A report sections

docs/papers/mascar_w18a_existing_kernel_log_audit.md must include:

1. Goal
2. Target phase-pending rows
3. Existing logs searched
4. Source locations inspected
5. Whether existing logs already contain kernel name/order
6. Parser scaffolding
7. Need for W18B default-off simulator trace
8. Risks and limitations

## Validation

Run:

- python3 -m py_compile experiments/common/gpgpusim_matrix/collect_kernel_trace.py
- python3 -m py_compile experiments/paper-mascar/workloads/matrix/W18/w18a_existing_log_trace_audit.py
- Run parser on at least one existing run directory if available
- git diff --check

## Stop conditions

Stop W18A only if:
1. manifests cannot be read
2. parser cannot be created
3. repository state becomes unsafe

Do not stop because existing logs lack kernel trace. That is expected and W18B should add default-off trace.
