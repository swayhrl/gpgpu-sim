# Mascar W24D Guidance: Refreshed Sweep Closeout and Review Pack

## Stage position

This is W24D, closeout for Table III refreshed sweep.

## Goal

Package W24 results, reports, manifests, and postchecks for GPT review.

## Required closeout files

Create:

- experiments/paper-mascar/workloads/audit/W24/w24_postcheck.md
- experiments/paper-mascar/workloads/audit/W24/w24_diff_name_status.txt
- experiments/paper-mascar/workloads/audit/W24/w24_symbol_grep.txt
- experiments/paper-mascar/workloads/matrix/W24/w24_closeout_manifest.csv

w24_closeout_manifest.csv columns:

- artifact
- path
- status
- description

Include:

- run plan
- config matrix
- workload manifests
- actual results
- analysis results
- coverage manifest
- report
- postcheck
- raw logs archive path

## Raw log handling

If results/W24 contains large run logs:

1. Archive raw run directories to:
   /workspace/tmp/mascar_w24_raw_runs_YYYYMMDD_HHMMSS.tar.gz

2. Remove raw runs directories from repo tree.

3. Append archive path to:
   - w24b_postcheck.md
   - w24_postcheck.md

Keep only:
- results.csv
- summary.md
- run_manifest.csv
- status_matrix.csv
- analysis CSVs
- reports

## Final report

Create or update:

- docs/papers/mascar_w24_tableiii_refreshed_sweep_report.md

Ensure it includes:
- final row counts
- ready/unavailable counts
- M2/M3/M4 active counts
- speedup/geomean caveats
- phase mapping caveats
- next step recommendations

## Validation

Run:

- git diff --check
- python3 -m py_compile experiments/paper-mascar/workloads/matrix/W24/prepare_w24_tableiii_run_plan.py
- python3 -m py_compile experiments/paper-mascar/workloads/matrix/W24/analyze_w24_tableiii_results.py
- bash -n experiments/paper-mascar/workloads/matrix/W24/run_w24_tableiii_refreshed_sweep.sh
- Check no __pycache__ remains.
- Check no raw huge runs dirs intended for git.
- Check review pack exists.

## Review pack

Create:

- /workspace/tmp/mascar_w24_tableiii_refreshed_review_pack_YYYYMMDD_HHMMSS.tar.gz

Include:

- docs/papers/mascar_w24*.md
- experiments/paper-mascar/workloads/matrix/W24/
- experiments/paper-mascar/workloads/results/W24/
- experiments/paper-mascar/workloads/audit/W24/
- full git diff patch

Do not include huge raw logs.

## Final report to GPT

Report only:

1. elapsed_sec
2. review pack path
3. git status --short
4. selected ready rows
5. actual run rows
6. completed / timeout / crash counts
7. M2/M3/M4 active counts
8. speedup/geomean summary
9. files GPT should review
