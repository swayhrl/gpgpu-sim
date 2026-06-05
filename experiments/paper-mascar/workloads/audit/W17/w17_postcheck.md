# W17 Postcheck

start_iso=2026-06-05T13:45:10+08:00
end_iso=2026-06-05T13:53:54+08:00
elapsed_sec=524
branch=hrl/paper/mascar-repro-v0
head=659a89c

## Git Status Before/After

Initial status was captured at W17 start; final status is in w17_diff_name_status.txt and final response.

## Commands Run

- W17A audit script
- W17B dry-run and actual build helper
- W17C dry-run all 30 wrappers
- W17C actual smoke for 9 promoted app-level wrappers
- W17D manifest/report generation

## Counts

build_attempts_count=28
wrapper_updates_count=1
ready_count_before=6
ready_count_after=15
placeholder_count_before=24
placeholder_count_after=15
phase_pending_count=9
actual_smoke_status=9/9 promoted app-level rows passed
raw_logs_archive=/workspace/tmp/mascar_w17_raw_logs_20260605_135428.tar.gz
review_pack_path=/workspace/tmp/mascar_w17_availability_review_pack_20260605_135441.tar.gz

## Warnings

No missing_binary row produced a valid ELF CUDA binary in W17B. Newly ready phase rows are app-level only and require W18 kernel launch trace.

supplemental_repo_raw_logs=/workspace/tmp/mascar_w17_repo_raw_logs_20260605_135502.tar.gz
