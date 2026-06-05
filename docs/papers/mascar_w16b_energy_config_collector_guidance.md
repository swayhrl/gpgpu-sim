# Mascar W16B Guidance: Energy Configs and Collector Support

## Stage position

This is W16B.

W16A audited energy/power infrastructure. W16B must create energy-enabled configs if possible and extend the common collector to parse current-simulator energy fields.

## Goal

1. Preserve existing collector fields, including W15 m3diag fields.
2. Add energy/power parsing to collector.
3. Create baseline and M4 energy config families if power options exist.
4. Provide fallback report if energy is unavailable.

## Hard constraints

1. Do not modify M1-M4 Mascar mechanism code.
2. Do not remove existing collector parsing.
3. Do not break W3-W15 result collection.
4. Energy config changes must be new config directories, not destructive edits to existing baseline/M4 configs.
5. New configs must clearly mark current-simulator energy trend only.
6. Do not run full benchmark suites.
7. Do not fabricate energy fields.

## Collector update

Modify:

- experiments/common/gpgpusim_matrix/collect_gpgpusim_stats.py

Required behavior:

- Keep existing parsed fields.
- Keep paper_mascar_m3diag_* support from W15.
- Add optional energy fields.
- If a field is absent, output blank or 0 consistently, and indicate availability in summary.

Add normalized output columns where possible:

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
- energy_fields_found

Parsing rules:

- Use regex patterns discovered in W16A.
- Support scientific notation.
- Be robust to either "name = value" or "name: value".
- Do not crash if fields are absent.
- Summary should report which energy fields appeared.

Also create a small standalone helper if useful:

- experiments/paper-mascar/energy/W16B/inspect_energy_log_fields.py

This helper can scan logs and print candidate energy/power lines.

## Config creation

If power/energy option names are found, create:

- configs/hrl-repro/SM7_QV100_mascar_energy_baseline_off/
- configs/hrl-repro/SM7_QV100_mascar_energy_m4_reexec_load_on/

Base them on:

- configs/hrl-repro/SM7_QV100_mascar_baseline_off/
- configs/hrl-repro/SM7_QV100_mascar_m4_reexec_load_on/

Add required power/energy knobs discovered from source/configs.

Examples, only if confirmed by grep:
- -power_simulation_enabled 1
- -power_simulation_mode ...
- -accelwattch_xml_file ...
- -gpuwattch_xml_file ...

If XML/config file is needed:
- copy path reference from existing configs if available
- do not invent nonexistent XML path
- if no XML is available, mark energy config incomplete

Each config directory must include README.md explaining:

- current-simulator energy trend config
- base config used
- power knobs enabled
- not paper-equivalent GPUWattch/GTX480
- known limitations

If power options are not found:
- do not create fake energy configs
- create:
  - docs/papers/mascar_w16b_energy_unavailable_note.md
  - experiments/paper-mascar/energy/W16B/energy_unavailable_reason.txt

## Config sanity script

Create:

- experiments/paper-mascar/energy/W16B/check_energy_configs.sh

Requirements:
- bash
- verify config dirs exist
- verify gpgpusim.config exists
- grep relevant power knobs
- print enabled/disabled summary
- exit 0 if energy unavailable but documented
- exit nonzero only for broken generated configs

## Required outputs

Create directory:

- experiments/paper-mascar/energy/W16B/

Outputs:

- docs/papers/mascar_w16b_energy_config_collector.md
- experiments/paper-mascar/energy/W16B/energy_collector_field_map.csv
- experiments/paper-mascar/energy/W16B/check_energy_configs.sh
- experiments/paper-mascar/energy/W16B/w16b_postcheck.md
- configs/hrl-repro/SM7_QV100_mascar_energy_baseline_off/ if feasible
- configs/hrl-repro/SM7_QV100_mascar_energy_m4_reexec_load_on/ if feasible
- energy unavailable note if not feasible

## Validation

Run:

- git diff --check
- python3 -m py_compile experiments/common/gpgpusim_matrix/collect_gpgpusim_stats.py
- python3 -m py_compile experiments/paper-mascar/energy/W16B/inspect_energy_log_fields.py if created
- bash -n experiments/paper-mascar/energy/W16B/check_energy_configs.sh
- bash experiments/paper-mascar/energy/W16B/check_energy_configs.sh

## Stop conditions

Stop only if:
1. collector breaks and cannot be restored
2. generated configs are invalid and cannot be corrected
3. energy feature requires broad simulator source changes
4. elapsed time exceeds 120 minutes before W16C

Do not stop because energy is unavailable. Document and continue to W16D closeout with unavailable status.
