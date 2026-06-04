# W4 Postcheck

This file supersedes the initial W4 postcheck classification after fixing the collector explicit-pass regex.

## Status

- classification_fix_applied: yes
- reason: original PASS regex matched ordinary lowercase English `pass` in simulator configuration comments
- corrected_result: lowercase `pass` false positives were removed; 2 `spmv` rows remain `completed_explicit_pass`, and 10 other ready rows are `completed_no_explicit_pass`

## Matrix Summary

- matrix_rows: 60
- completed_explicit_pass_rows: 2
- completed_no_explicit_pass_rows: 10
- completed_stats_found_rows: 0
- completed_nonzero_with_stats_rows: 0
- unavailable_placeholder_rows: 48
- phase_unknown_rows: 18
- missing_binary_rows: 28
- missing_source_rows: 2
- timeout_rows: 0
- crash_rows: 0

## Validation After Fix

- collector rerun on dry-run output: done
- collector rerun on actual smoke output: done
- latest results/summary/status matrix/run manifest regenerated: done

## Warnings

- Only the two `spmv` rows have explicit benchmark pass markers; the other ready rows do not.
- M4 active smoke completed but did not trigger nonzero M4 re-execution counters.
- This remains a smoke coverage pass, not a performance or correctness-signoff run.
