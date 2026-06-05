# W21 Final Postcheck

start_ts=1780647788
end_ts=1780648287
elapsed_sec=499
start_iso=2026-06-05T16:23:08+08:00
end_iso=2026-06-05T16:31:27+08:00
branch=hrl/paper/mascar-repro-v0
head=04f7888
postcheck_method=start_ts=$(date +%s) / end_ts=$(date +%s)

## Counts
blocker_workloads=28
attempt_coverage=all blockers have at least two attempts
w21a_rows=78
w21b_rows=82
w21c_rows=56
w21d_data_rows=56
w21d_cmd_rows=56
parboil_binary_recovered_benchmarks=9
parboil_binary_recovered_names=cp;cutcp;lbm;mri-gridding;pns;rpes;sad;simple_mm;tpacf
newly_ready_workloads=0
ready_candidates_needing_smoke=1
still_unavailable_rows=27
status_counts={'still_unavailable': 8, 'binary_or_dependency_probe_passed_command_unverified': 7, 'ready_candidate_needs_collector_smoke': 1, 'binary_recovered_command_data_unverified': 9, 'parboil_build_unresolved': 3}
blocker_counts={'command/data not normalized in workload manifest': 2, 'no verified executable binary': 6, 'command_or_data_unverified': 7, 'not_promoted_without_full_gpgpusim_stats_smoke': 1, 'missing_or_unverified_data_and_run_command': 9, 'python2_driver_or_cuda11_source_compat_or_missing_data': 3}
raw_log_archive=/workspace/tmp/mascar_w21_raw_logs_20260605_163102.tar.gz

## Validation
- git diff --check: pass
- py_compile W21/W20 suite scripts: pass
- bash -n run_full_suite_matrix.sh: pass
- CSV headers complete: pass
- every blocker workload has >=2 attempts: pass
- ready subset dry-run/smoke executed: pass
- raw logs archived to /workspace/tmp: pass

## Notes
No Mascar M1-M4 mechanism behavior was modified. Parboil workload-side `common/Makefile.conf` was created/updated to enable local direct-make attempts.
