# Mascar W25E Guidance: Energy Field Recovery and W16-vs-W25 Diagnosis

## Stage position

W25E is a focused fix round after W25/W26.

W16 showed current-simulator power fields were parseable:
- kernel_avg_power
- gpu_tot_avg_power
- kernel_max_power
- gpu_tot_max_power

W25 ran 12/12 energy matrix rows but collected:
- true power fields = 0
- true energy fields = 0
- derived energy unavailable

W25E must determine why W16 saw power fields but W25 did not, and fix the issue if possible.

## Goal

Recover W25 power-field collection or produce a precise diagnosis.

This round should answer:

1. Did W25 actually run with energy configs?
2. Did W25 logs contain power fields but collector did not scan them?
3. Did AccelWattch output power report files into a different directory?
4. Did W25 config override fail and fall back to non-energy config?
5. Did W25 use wrappers that changed working directory and left power files outside run_dir?
6. Did W25 collector lose W16 parsing support?
7. Is the AccelWattch XML path valid in W25 run dirs?
8. If fixed, does W25 mini-rerun produce kernel_avg_power / gpu_tot_avg_power again?

## Hard constraints

1. Do not create a new branch.
2. Do not fetch upstream.
3. Do not modify Mascar M1-M4 mechanism behavior.
4. Preserve W15 m3diag collector fields.
5. Preserve W16 power collector fields.
6. Do not run a large energy matrix.
7. Only run a minimal diagnostic subset:
   - preferably spmv with energy_baseline_off and energy_m4_reexec_load
   - optionally one more workload if needed
8. Every actual run must use timeout.
9. Raw logs must be archived under /workspace/tmp if large.
10. Do not use git add . or git add -A.
11. Do not commit W25E outputs.
12. Use start_ts/end_ts in postcheck.
13. If power cannot be recovered, provide a precise blocker and next action.

## Inputs to compare

Read W16 files:

- docs/papers/mascar_w16_energy_trend_report.md
- docs/papers/mascar_w16b_energy_config_collector.md
- experiments/paper-mascar/energy/W16C/w16_energy_latest_results.csv
- experiments/paper-mascar/energy/W16C/w16_energy_latest_run_manifest.csv
- experiments/paper-mascar/energy/W16D/w16_energy_trend_summary.csv
- configs/hrl-repro/SM7_QV100_mascar_energy_baseline_off/gpgpusim.config
- configs/hrl-repro/SM7_QV100_mascar_energy_m4_reexec_load_on/gpgpusim.config

Read W25 files:

- docs/papers/mascar_w25_integrated_energy_sweep_report.md
- experiments/paper-mascar/energy/W25/results/w25_energy_latest_results.csv
- experiments/paper-mascar/energy/W25/results/w25_energy_latest_run_manifest.csv
- experiments/paper-mascar/energy/W25/results/w25_energy_actual_results.csv
- experiments/paper-mascar/energy/W25/matrix/run_w25_energy_sweep.sh
- experiments/paper-mascar/energy/W25/matrix/w25_energy_config_matrix.csv
- experiments/paper-mascar/energy/W25/matrix/w25_energy_workload_manifest.csv
- experiments/common/gpgpusim_matrix/collect_gpgpusim_stats.py

If raw W16 or W25 run logs were archived in /workspace/tmp, use their archive paths from postcheck when available.

## Required directories

Create:

- experiments/paper-mascar/energy/W25E/
- experiments/paper-mascar/energy/W25E/audit/
- experiments/paper-mascar/energy/W25E/results/
- experiments/paper-mascar/energy/W25E/matrix/

## W25E-A: Static comparison

Create script:

- experiments/paper-mascar/energy/W25E/audit/compare_w16_w25_energy_setup.py

Responsibilities:

1. Compare W16 and W25 config matrices.
2. Compare W16 and W25 gpgpusim.config files.
3. Check energy-related knobs:
   - power_simulation_enabled
   - power_simulation_mode
   - accelwattch_xml_file
   - gpuwattch_xml_file
   - any known power option discovered in W16
4. Check XML paths exist.
5. Check run manifests:
   - config_id
   - config_path
   - run_dir
   - working_dir
   - command
   - env
6. Check whether W25 used the intended energy config.
7. Compare collector field list / parser patterns.

Outputs:

- experiments/paper-mascar/energy/W25E/audit/w25e_static_comparison.csv
- experiments/paper-mascar/energy/W25E/audit/w25e_config_diff_summary.md
- experiments/paper-mascar/energy/W25E/audit/w25e_collector_field_check.md

## W25E-B: Log/file discovery

Create script:

- experiments/paper-mascar/energy/W25E/audit/find_energy_outputs.py

Responsibilities:

1. Scan W16 result dirs and W25 result dirs if present.
2. Scan run dirs, workload working dirs, copied run dirs, stdout/stderr/combined logs.
3. Search for patterns:
   - kernel_avg_power
   - gpu_tot_avg_power
   - kernel_max_power
   - gpu_tot_max_power
   - AccelWattch
   - power
   - gpuwattch
   - energy
   - total power
   - average power
4. Search for possible power output files:
   - *power*
   - *watt*
   - *.xml
   - *.log
   - *.out
5. Output concise file hits.

Outputs:

- experiments/paper-mascar/energy/W25E/audit/w25e_energy_output_hits.csv
- experiments/paper-mascar/energy/W25E/audit/w25e_energy_output_summary.md

## W25E-C: Minimal rerun

Create:

- experiments/paper-mascar/energy/W25E/matrix/w25e_energy_debug_config_matrix.csv
- experiments/paper-mascar/energy/W25E/matrix/w25e_energy_debug_workload_manifest.csv
- experiments/paper-mascar/energy/W25E/matrix/run_w25e_energy_debug.sh

Use only:

Workloads:
- spmv first
- optionally mri_q if spmv fails

Configs:
- energy_baseline_off
- energy_m4_reexec_load

Requirements:
- force run directory to keep all files
- save stdout/stderr/combined
- save env snapshot
- save resolved config path
- save working directory
- after run, copy any power-related files from workload working directory back into run_dir/power_artifacts/
- then run collector on run_dir

If common runner does not capture workload working dir power artifacts, patch runner or wrapper minimally to copy power files.

Likely fix candidates:

1. Collector scan scope:
   - extend collect_gpgpusim_stats.py to scan all text files under run_dir, not only combined.log.
   - include power_artifacts/ files.

2. Runner config override:
   - ensure GPGPUSIM_CONFIG_OVERRIDE and MASCAR_CONFIG_DIR are passed correctly.
   - ensure wrappers do not overwrite gpgpusim.config in workload cwd with non-energy config.

3. Power artifact location:
   - ensure run wrapper copies back files matching *power*, *wattch*, *accelwattch*, *gpuwattch*.

4. XML path:
   - ensure accelwattch XML path is absolute or copied into run dir and referenced correctly.

Do not modify Mascar mechanism.

## W25E-D: Diagnosis and report

Create:

- docs/papers/mascar_w25e_energy_field_recovery_report.md
- experiments/paper-mascar/energy/W25E/w25e_postcheck.md
- experiments/paper-mascar/energy/W25E/w25e_final_diagnosis.csv
- experiments/paper-mascar/energy/W25E/w25e_diff_name_status.txt
- experiments/paper-mascar/energy/W25E/w25e_symbol_grep.txt

w25e_final_diagnosis.csv columns:

- issue
- evidence
- fix_applied
- verification_status
- remaining_risk
- next_action

Diagnosis categories:

- config_not_energy_enabled
- xml_path_invalid
- config_override_failed
- power_output_in_workload_dir_not_run_dir
- collector_scan_scope_too_narrow
- collector_regex_missing
- power_fields_not_emitted_by_sim
- unresolved

Report sections:

1. Goal
2. W16 observed power fields
3. W25 missing power fields
4. Static config comparison
5. Log/output discovery
6. Minimal rerun result
7. Fixes applied
8. Whether power fields recovered
9. Updated energy pipeline behavior
10. Remaining limitations
11. Next recommended action

## Review pack

Create:

- /workspace/tmp/mascar_w25e_energy_recovery_review_pack_YYYYMMDD_HHMMSS.tar.gz

Include:
- W25E reports
- W25E audit CSV/MD
- W25E minimal rerun results summaries
- changed runner/collector/config/wrapper files
- postcheck
- full git diff patch

Do not include large raw logs.

## Validation

Run:

- git diff --check
- python3 -m py_compile experiments/paper-mascar/energy/W25E/audit/compare_w16_w25_energy_setup.py
- python3 -m py_compile experiments/paper-mascar/energy/W25E/audit/find_energy_outputs.py
- python3 -m py_compile experiments/common/gpgpusim_matrix/collect_gpgpusim_stats.py
- bash -n experiments/paper-mascar/energy/W25E/matrix/run_w25e_energy_debug.sh
- Run static comparison.
- Run output discovery.
- Run minimal rerun if configs are available.
- Run collector.
- Check if kernel_avg_power or gpu_tot_avg_power appears in W25E results.

## Final report to GPT

Report only:
1. elapsed_sec
2. review pack path
3. git status --short
4. root cause
5. fix applied
6. whether power fields recovered
7. if recovered, minimal-rerun power fields summary
8. files GPT should review

## Stop conditions

Stop only if:
1. build cannot be restored
2. minimal rerun cannot run due global framework breakage
3. collector cannot be restored
4. repository state becomes unsafe

Do not stop because the first rerun lacks power fields. Diagnose and try at least two fixes:
- scan scope/artifact copy fix
- config/XML/override fix
