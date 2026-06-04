# Mascar W8B Postcheck

- start_ts: 1780591778
- end_ts: 1780595336
- elapsed_sec: 3558
- branch: hrl/paper/mascar-repro-v0
- HEAD: 6921aee366c293f74df57a9bef129a980c98936d

## Execution

- dry-run outdir: `experiments/paper-mascar/workloads/results/W8/w8_focused_dryrun_configfix`
- actual outdir: `experiments/paper-mascar/workloads/results/W8/w8_focused_actual_configfix`
- run plan rows: 12
- actual result rows: 12
- completed explicit pass: 4
- completed stats found: 8
- timeout/crash rows: 0

## Debug

W8 found and fixed a wrapper/config override bug in direct workload wrappers. Actual binary runs now temporarily install the selected `MASCAR_CONFIG_DIR` config files into the workload CWD and restore the original files after execution.

The final actual run confirms `baseline_off` uses `paper_mascar_enabled = 0` and zero M1-M4 counters. No M1-M4 mechanism code was changed.

## Validation

- `git diff --check`: pass
- `bash -n` W8 runner and common wrapper: pass
- `python3 -m py_compile` W8 planner/analyzer: pass
- CSV row checks: pass

## Git Status Snapshot

```text
 M experiments/paper-mascar/workloads/wrappers/_run_table_iii_common.sh
?? experiments/paper-mascar/workloads/matrix/W8/
?? experiments/paper-mascar/workloads/results/W8/
```
