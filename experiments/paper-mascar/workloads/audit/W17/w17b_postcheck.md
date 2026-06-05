# W17B Postcheck

start_ts=1780638310
end_ts=1780638528
elapsed_sec=218

## Checks

- build helper syntax checked with bash -n.
- W17_DRY_RUN=1 helper run completed.
- actual build/recovery attempted with timeout.
- build_results.csv, binary_recovery_results.csv, and data_availability_results.csv generated.

## Result

No new missing_binary workload became binary-ready in W17B. All missing_binary rows have recorded native and fallback attempts.

build_log_dir=/workspace/tmp/mascar_w17_build_logs_20260605_134816
