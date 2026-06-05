# W16D Postcheck

start_ts=1780629197
end_ts=1780637256
elapsed_sec=8059

## Required Artifacts

- experiments/paper-mascar/energy/W16D/analyze_w16_energy.py
- experiments/paper-mascar/energy/W16D/w16_energy_trend_summary.csv
- experiments/paper-mascar/energy/W16D/w16_energy_ratio_by_workload.csv
- experiments/paper-mascar/energy/W16D/w16_energy_availability_matrix.csv
- experiments/paper-mascar/energy/W16D/w16_energy_summary.md
- docs/papers/mascar_w16_energy_trend_report.md
- experiments/paper-mascar/energy/W16D/w16_diff_name_status.txt
- experiments/paper-mascar/energy/W16D/w16_symbol_grep.txt
- experiments/paper-mascar/energy/W16D/w16_closeout_manifest.csv

## Result

Real power fields were found and parsed for 6/6 final smoke rows. Direct energy fields were unavailable; derived current-simulator energy trend was computed from average power and cycles.

## Raw Logs Archive

Large W16 per-run raw logs were not committed to git. They were archived locally at:

```text
/workspace/tmp/mascar_w16_raw_runs_20260605_133325.tar.gz
```
