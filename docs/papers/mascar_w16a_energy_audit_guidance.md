# Mascar W16A Guidance: Energy / Power Infrastructure Audit

## Stage position

This is W16A of Mascar current-simulator energy reproduction.

Current state:
- M1-M4 Mascar mechanisms are implemented.
- W15 M3 diagnostic has completed or should be completed before W16 starts.
- W15 modified experiments/common/gpgpusim_matrix/collect_gpgpusim_stats.py to parse paper_mascar_m3diag_* counters.
- W16 will extend collector support for power/energy fields.
- W16 must preserve W15 m3diag collector support.

W16 is not a paper-exact GPUWattch reproduction. It is a current-simulator energy trend pipeline.

## Goal

Audit whether the current repository/config environment can produce energy/power statistics.

The target is:
- discover available AccelWattch / GPUWattch / power model knobs
- identify config/XML files required for energy simulation
- identify energy/power log fields printed by current simulator
- create a map from raw log field names to normalized collector fields
- decide whether W16B/W16C can run energy trend experiments

## Paper context

The Mascar paper reports energy using GPUWattch on GPGPU-Sim v3.2.2 and GTX480/Fermi. Current repository is GPGPU-Sim 4.x style and may use AccelWattch or different power output. Therefore W16 must not claim paper-equivalent 12% energy saving.

## Hard constraints

1. Do not create a new branch.
2. Do not fetch upstream.
3. Do not modify M1-M4 Mascar mechanism code.
4. Do not delete or regress paper_mascar_m3diag_* parsing in the collector.
5. Do not run full benchmark suites in W16A.
6. Do not invent energy fields. If not present, mark unavailable.
7. Do not commit W16 outputs.
8. Do not use git add . or git add -A.
9. Use start_ts/end_ts in postcheck.
10. Large logs must be archived under /workspace/tmp and not committed.

## Files and search targets

Search these paths first:

- README.md
- CHANGES
- setup_environment
- configs/
- src/gpgpu-sim/
- src/accelwattch/
- src/power/
- src/
- experiments/
- docs/

Search terms:

- AccelWattch
- GPUWattch
- power_simulation
- power_simulation_enabled
- power_simulation_mode
- accelwattch
- gpuwattch
- xml
- energy
- leakage
- dynamic
- total_energy
- total power
- average power
- g_power_config_name
- option_parser_register
- print_power
- print_energy
- XML

Use grep commands and save concise results.

## Required outputs

Create directory:

- experiments/paper-mascar/energy/W16A/

Create:

- docs/papers/mascar_w16a_energy_infrastructure_audit.md
- experiments/paper-mascar/energy/W16A/energy_option_grep.txt
- experiments/paper-mascar/energy/W16A/energy_config_candidates.txt
- experiments/paper-mascar/energy/W16A/energy_log_field_candidates.txt
- experiments/paper-mascar/energy/W16A/e1_energy_stat_map.csv
- experiments/paper-mascar/energy/W16A/w16a_postcheck.md

## e1_energy_stat_map.csv format

Columns:

- normalized_field
- raw_pattern
- source_file_or_log
- parser_regex
- unit_guess
- availability
- notes

Normalized fields to attempt:

- energy_total
- energy_dynamic
- energy_leakage
- energy_dram
- energy_l1
- energy_l2
- energy_cache
- energy_interconnect
- energy_core
- power_total_avg
- power_peak
- runtime_cycles
- gpu_tot_ipc

If a field cannot be found, keep row with availability=unavailable.

## Energy config audit

Identify whether existing configs already contain power/energy knobs.

If knobs are found, record exact names and values.

Do not assume exact option names. Inspect source/configs.

Likely option names may include:
- power_simulation_enabled
- power_simulation_mode
- gpuwattch_xml_file
- accelwattch_xml_file

But confirm by grep before using.

## Minimal log audit

If an obvious prior run log exists under experiments/paper-mascar/workloads/results or m5/m6/w8/w11/w12 logs, grep for energy/power fields.

If no logs contain energy fields, that is acceptable; W16B will try enabling power.

## W16A report

docs/papers/mascar_w16a_energy_infrastructure_audit.md must include:

1. Goal
2. Current simulator version/config caveat
3. Power/energy related options found
4. Config/XML candidates found
5. Energy field candidates found
6. Collector changes needed
7. Whether W16B can run energy smoke
8. Risks and unknowns
9. Explicit statement that this is not paper-exact GPUWattch reproduction

## Validation

Run:

- git diff --check
- python3 -m py_compile experiments/common/gpgpusim_matrix/collect_gpgpusim_stats.py if touched
- Check W16A CSV headers
- Check report exists

## Stop conditions

Stop only if:
1. repository state becomes unsafe
2. setup/build is broken before W16 begins
3. no Python available
4. energy option search cannot be completed due filesystem/tool failure

Do not stop just because energy fields are not found. Mark unavailable and continue to W16B guidance.
