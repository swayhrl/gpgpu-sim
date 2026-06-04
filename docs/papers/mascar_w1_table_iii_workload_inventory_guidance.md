# Mascar W1 Detailed Guidance: Table III Workload Inventory and Availability Audit

## Stage position

This is W1 of the post-Mascar-mechanism workload coverage effort.

Current state:
- M1 implemented L1 saturation probe.
- M2 implemented EP/MP owner scheduling.
- M3 implemented non-owner L1 hit-only / miss-NACK.
- M4 implemented load-only re-execution queue.
- M5/M6 completed focused runtime validation on rodinia_hotspot.
- W0 has committed and tagged the current Mascar mechanism/focused-validation closeout.

W1+W2 now moves from "one workload validation" to "paper workload coverage infrastructure".

W1 must build the canonical Table III workload inventory and audit local availability.
W2 will normalize runnable commands and wrappers.

Do not modify Mascar mechanism code in W1.
Do not run full simulations in W1.
Do not claim workload availability without checking filesystem evidence.

## Paper target

The Mascar paper Table III evaluates 30 kernels from Rodinia and Parboil.

Canonical workload list:

Memory-intensive:
1. BP-2, M, 19
2. bfs, M, 2.4
3. histo-1, M, 15.3
4. histo-3, M, 14.3
5. kmeans-1, M, 0.27
6. lbm, M, 10.4
7. leuko-1, M, 28
8. mrig-1, M, 13.4
9. mrig-2, M, 22.4
10. mummer, M, 4.9
11. particle, M, 3.5
12. sad-2, M, 10.2
13. spmv, M, 4.2
14. srad-1, M, 25
15. srad-2, M, 22

Compute-intensive:
16. BP-1, C, 107
17. cutcp, C, 949
18. histo-2, C, 35.9
19. histogram, C, 91.2
20. kmeans-2, C, 88
21. lavaMD, C, 512
22. leuko-2, C, 173
23. leuko-3, C, 5289
24. mri-q, C, 3479
25. mrig-3, C, 147
26. pathfinder, C, 55.83
27. sad-1, C, 2563
28. sgemm, C, 75
29. stencil, C, 38
30. tpacf, C, 10816

Important:
- Some paper entries are different kernels/phases of the same application, not necessarily separate directories.
- Do not collapse BP-1/BP-2, histo-1/2/3, kmeans-1/2, leuko-1/2/3, mrig-1/2/3, sad-1/2, srad-1/2 into a single workload row.
- If the local benchmark repo only has one command for an app, keep the paper rows separate and mark phase mapping as unknown until resolved.

## Required output directories

Create:

- experiments/paper-mascar/workloads/
- experiments/paper-mascar/workloads/audit/
- experiments/paper-mascar/workloads/scripts/
- experiments/paper-mascar/workloads/wrappers/

## Required canonical manifest

Create:

- experiments/paper-mascar/workloads/mascar_table_iii_workload_manifest.csv

Required columns:

- paper_id
- paper_name
- paper_suite
- paper_app
- paper_kernel_or_phase
- paper_type
- paper_inst_per_l1_miss
- local_suite_guess
- local_app_guess
- local_path
- local_binary
- local_data_path
- local_command
- local_input_size
- phase_mapping_status
- availability_status
- build_status
- run_command_status
- expected_runtime_class
- notes

Allowed availability_status values:

- available
- available_needs_build
- partial_phase_unknown
- missing_binary
- missing_data
- missing_source
- unsupported_env
- unknown

Allowed phase_mapping_status values:

- exact
- approximate
- app_only
- unknown
- not_found

Allowed expected_runtime_class values:

- tiny
- short
- medium
- long
- unknown

Populate all 30 paper rows exactly.

## Required audit scripts

Create:

- experiments/paper-mascar/workloads/scripts/audit_table_iii_workloads.py

Purpose:
- Read the canonical manifest.
- Search local candidate roots for matching source/binary/data.
- Update an audited manifest without destroying the hand-written canonical paper fields.

Candidate roots to search:

- /workspace/repos/gpgpu-workloads
- /workspace/repos/gpgpu-sim_simulations
- /workspace/repos/gpgpu-sim_distribution
- /workspace/repos
- /workspace

Search terms per workload should include lower-case aliases. Examples:
- BP-1/BP-2: backprop, bp
- bfs: bfs
- histo-1/2/3/histogram: histo, histogram
- kmeans-1/2: kmeans
- lavaMD: lava, lavaMD, lavamd
- leuko-1/2/3: leukocyte, leuko
- mri-q: mri-q, mriq
- mrig-1/2/3: mri-gridding, mrig, mri-gridding
- mummer: mummer
- particle: particlefilter, particle
- pathfinder: pathfinder
- sad-1/2: sad
- sgemm: sgemm
- spmv: spmv
- srad-1/2: srad
- stencil: stencil
- tpacf: tpacf
- cutcp: cutcp
- lbm: lbm

The script should:
- avoid scanning enormous hidden directories.
- skip .git, build artifacts, logs, review packs.
- cap output size.
- write:
  - experiments/paper-mascar/workloads/mascar_table_iii_workload_manifest_audited.csv
  - experiments/paper-mascar/workloads/audit/workload_candidate_paths.txt
  - experiments/paper-mascar/workloads/audit/workload_availability_summary.md

Do not delete or overwrite the canonical manifest except intentionally and safely.

## Required report

Create:

- docs/papers/mascar_w1_workload_inventory_report.md

Required sections:

1. Goal
2. Paper Table III target
3. Canonical workload list
4. Local search roots
5. Availability summary
6. Workloads available now
7. Workloads requiring build
8. Workloads missing binary/data/source
9. Phase-mapping uncertainties
10. Recommended W2 actions
11. Limitations

## Required machine-readable summary

Create:

- experiments/paper-mascar/workloads/mascar_workload_availability_summary.csv

Required columns:

- paper_id
- paper_name
- paper_type
- availability_status
- phase_mapping_status
- local_path
- local_binary
- local_data_path
- recommended_action

## Validation

Run:

- python3 experiments/paper-mascar/workloads/scripts/audit_table_iii_workloads.py --help
- python3 experiments/paper-mascar/workloads/scripts/audit_table_iii_workloads.py
- python3 -m py_compile experiments/paper-mascar/workloads/scripts/audit_table_iii_workloads.py

Check:

- canonical manifest has exactly 30 data rows.
- audited manifest has exactly 30 data rows.
- no paper row is lost.
- all CSV headers are present.
- report mentions all 30 workloads or groups them clearly.

## W1 notes

Create:

- experiments/paper-mascar/workloads/audit/w1_postcheck.md

Include:

- branch and HEAD
- start/end/elapsed if W1 is run independently
- commands run
- manifest row counts
- summary of availability
- warnings

If W1 and W2 are run together, W2 final postcheck may supersede this, but still create W1 notes.

## Stop conditions

Stop before W2 only if:

1. canonical manifest cannot be created.
2. audit script corrupts or loses workload rows.
3. repository state becomes unclear.
4. Python tooling unavailable.

Do not stop merely because many workloads are missing. Missing workloads are expected and must be documented.
