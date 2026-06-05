# Mascar W25D Guidance: Energy Sweep Closeout

## Stage position

This is W25D, closeout for W25 integrated energy sweep.

## Goal

Package W25 run plan, results, analysis, and report for GPT review.

## Required closeout files

Create:

- experiments/paper-mascar/energy/W25/audit/w25_postcheck.md
- experiments/paper-mascar/energy/W25/audit/w25_diff_name_status.txt
- experiments/paper-mascar/energy/W25/audit/w25_symbol_grep.txt
- experiments/paper-mascar/energy/W25/matrix/w25_closeout_manifest.csv

## w25_postcheck.md requirements

Include:
- start_iso
- end_iso
- elapsed_sec
- branch and HEAD
- W25A/B/C status
- workloads selected
- configs run
- actual run row count
- completed/timeout/crash count
- energy fields found
- derived energy method
- key ratio summary
- raw logs archive path
- review pack path
- warnings

## Review pack

Create:

- /workspace/tmp/mascar_w25_energy_integrated_review_pack_YYYYMMDD_HHMMSS.tar.gz

Include:
- docs/papers/mascar_w25*.md
- experiments/paper-mascar/energy/W25/
- changed collector if any
- full git diff patch

Do not include huge raw logs.

## Validation

Run:
- git diff --check
- python3 -m py_compile experiments/paper-mascar/energy/W25/matrix/prepare_w25_energy_run_plan.py
- python3 -m py_compile experiments/paper-mascar/energy/W25/matrix/analyze_w25_energy_results.py
- bash -n experiments/paper-mascar/energy/W25/matrix/run_w25_energy_sweep.sh
- Check no __pycache__ remains.
- Check no large raw run dirs intended for git.
- Check review pack exists.

## Final report to GPT for W25

Include:
1. W25 elapsed_sec
2. energy review pack path
3. git status --short
4. workloads/configs run
5. whether energy/power fields were found
6. key ratio summary
7. caveats
