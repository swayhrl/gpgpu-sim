# Mascar W25F Guidance: Energy Rerun Closeout and Review Pack

## Stage position

This is W25F-D.

W25F-A planned, W25F-B reran, W25F-C analyzed. W25F-D packages results for GPT review.

## Goal

Create clean closeout artifacts and review pack.

## Required closeout files

Create:

- experiments/paper-mascar/energy/W25F/w25f_postcheck.md
- experiments/paper-mascar/energy/W25F/w25f_diff_name_status.txt
- experiments/paper-mascar/energy/W25F/w25f_symbol_grep.txt
- experiments/paper-mascar/energy/W25F/matrix/w25f_closeout_manifest.csv
- docs/papers/mascar_w25f_energy_rerun_closeout.md

## w25f_postcheck.md requirements

Include:

- start_iso
- end_iso
- elapsed_sec
- branch and HEAD
- W25F-A/B/C status
- workloads/configs run
- actual run row count
- completed/timeout/crash count
- rows with power artifacts
- rows with kernel_avg_power
- rows with gpu_tot_avg_power
- derived energy method
- key ratio summary
- raw logs archive path
- review pack path
- warnings

## Raw logs handling

If W25F result directories contain raw run logs:

- archive raw run dirs to:
  /workspace/tmp/mascar_w25f_raw_runs_YYYYMMDD_HHMMSS.tar.gz

- remove run dirs from repo if large

Keep:

- results.csv
- summary.md
- status_matrix.csv
- run_manifest.csv
- power_artifact_check.csv
- trend CSVs
- reports

## Review pack

Create:

- /workspace/tmp/mascar_w25f_energy_rerun_review_pack_YYYYMMDD_HHMMSS.tar.gz

Include:

- W25F matrix scripts
- W25F manifests
- W25F results summaries
- W25F power artifact check
- W25F analysis CSVs
- W25F reports
- changed common runner/collector only if modified in this round
- full git diff patch

Do not include huge raw logs.

## Final validation

Run:

- git diff --check
- python3 -m py_compile experiments/paper-mascar/energy/W25F/matrix/prepare_w25f_energy_rerun_plan.py
- python3 -m py_compile experiments/paper-mascar/energy/W25F/matrix/analyze_w25f_energy_rerun.py
- bash -n experiments/paper-mascar/energy/W25F/matrix/run_w25f_energy_rerun.sh
- python3 -m py_compile experiments/common/gpgpusim_matrix/collect_gpgpusim_stats.py
- Check no __pycache__
- Check review pack exists

## Final report to GPT

Report only:

1. elapsed_sec
2. review pack path
3. git status --short
4. actual run rows completed/timeout/crash
5. rows with kernel_avg_power / gpu_tot_avg_power
6. derived energy ratio summary
7. whether runner/collector fixes were needed
8. files GPT should review
