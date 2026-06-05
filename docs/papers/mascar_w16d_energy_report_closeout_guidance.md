# Mascar W16D Guidance: Energy Trend Report and Closeout

## Stage position

This is W16D, the closeout stage for current-simulator energy trend.

## Goal

Create a clear energy trend report based on W16C.

If energy fields are available:
- compare baseline vs M4 energy on selected workloads
- compute relative energy ratios
- provide caveated interpretation

If energy fields are unavailable:
- create an energy-unavailable report
- explain what is missing
- provide next steps for enabling AccelWattch/current power model

## Analysis script

Create:

- experiments/paper-mascar/energy/W16D/analyze_w16_energy.py

Inputs:
- W16C latest results CSV
- W16C run manifest
- W16A stat map

Outputs:
- experiments/paper-mascar/energy/W16D/w16_energy_trend_summary.csv
- experiments/paper-mascar/energy/W16D/w16_energy_ratio_by_workload.csv
- experiments/paper-mascar/energy/W16D/w16_energy_availability_matrix.csv
- experiments/paper-mascar/energy/W16D/w16_energy_summary.md

Calculations if fields exist:

For each workload:
- baseline cycles
- M4 cycles
- baseline IPC
- M4 IPC
- baseline energy_total
- M4 energy_total
- energy_ratio = M4 / baseline
- energy_saving = 1 - energy_ratio
- power ratio if available
- DRAM/cache/leakage ratios if available

Only compute ratio when both baseline and M4 have valid values.

If field absent:
- mark unavailable
- do not compute ratio

## Report

Create:

- docs/papers/mascar_w16_energy_trend_report.md

Required sections:

1. Executive summary
2. Scope and caveat
3. Difference from paper GPUWattch evaluation
4. Energy infrastructure audit
5. Configs used
6. Workloads used
7. Energy field availability
8. Baseline vs M4 trend table
9. Missing fields and unavailable cases
10. Interpretation
11. Next steps for stricter energy reproduction
12. Reuse for future papers

Mandatory language:
- This is current-simulator energy trend only.
- This is not a paper-equivalent GPUWattch/GTX480 result.
- Do not claim reproduction of the paper's 12% energy saving unless a comparable legacy setup is used.

## Closeout artifacts

Create:

- experiments/paper-mascar/energy/W16D/w16d_postcheck.md
- experiments/paper-mascar/energy/W16D/w16_diff_name_status.txt
- experiments/paper-mascar/energy/W16D/w16_symbol_grep.txt
- experiments/paper-mascar/energy/W16D/w16_closeout_manifest.csv

w16_closeout_manifest.csv columns:
- artifact
- path
- status
- description

## Review pack

Create:

- /workspace/tmp/mascar_w16_energy_review_pack_YYYYMMDD_HHMMSS.tar.gz

Include:
- W16 docs
- W16 energy configs if created
- collector changes
- W16A/B/C/D experiment files
- results CSV/MD
- postcheck
- full git diff patch

Do not include huge raw logs in git. Review pack may include compressed summaries but should avoid huge raw logs unless necessary.

## Final validation

Run:

- git diff --check
- python3 -m py_compile experiments/common/gpgpusim_matrix/collect_gpgpusim_stats.py
- python3 -m py_compile experiments/paper-mascar/energy/W16D/analyze_w16_energy.py
- bash -n experiments/paper-mascar/energy/W16C/run_w16_energy_matrix.sh if created
- bash -n experiments/paper-mascar/energy/W16B/check_energy_configs.sh if created

## Final report to GPT

Report only:
1. elapsed_sec
2. review pack path
3. git status --short
4. whether energy fields were found
5. workloads/configs run
6. key energy trend or unavailable reason
7. files GPT should review
