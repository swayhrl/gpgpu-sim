# W20 Final Postcheck

start_ts=1780645168
end_ts=1780646679
elapsed_sec=1511
start_iso=2026-06-05T15:39:28+08:00
end_iso=2026-06-05T16:04:39+08:00
branch=hrl/paper/mascar-repro-v0
head=c63cc49
postcheck_method=start_ts=$(date +%s) / end_ts=$(date +%s)

## Row counts
full_suite_manifest_rows=41
ready_rows=13
dryrun_rows=41
smoke_rows=13
blocker_rows=28
ready_by_suite=parboil:5;rodinia:8
blocker_categories=binary_available_command_unverified:4;failed_smoke:4;source_available_missing_binary:20

## Validation
- git diff --check: pass
- py_compile normalizer/collector/common collector: pass
- bash -n run_full_suite_matrix.sh: pass
- dry-run row count equals normalized manifest row count: pass
- smoke row count equals ready row count: pass
- raw run directories in repo: 0
- __pycache__ directories after cleanup: 0

## Raw log archive
/workspace/tmp/mascar_w20_raw_runs_20260605_160017.tar.gz;/workspace/tmp/mascar_w20_raw_runs_extra_20260605_160359.tar.gz

## Notes
W20 fixed runner/manifest metadata handling for app_name. No Mascar M1-M4 behavior was modified.

## Review Fix

Fixed W20 ready/suite summary metadata:
- `w20_ready_summary.csv` now uses `correctness_status_w20`.
- `w20_suite_summary.csv` now counts `completed_stats_found` separately.
- Raw W20 run results were not changed.
