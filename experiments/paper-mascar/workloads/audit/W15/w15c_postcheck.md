# W15C Postcheck

- start_iso: 2026-06-05T10:04:11+08:00
- end_iso: 2026-06-05T10:35:06+08:00
- elapsed_sec: 1855
- microbenchmark_path: experiments/paper-mascar/workloads/micro/mascar_m3_diag
- build_result: success after switching micro build to dynamic CUDA runtime with -cudart shared
- strategies_attempted: 5
- final_strategy_results: all 5 completed_exit0
- m3_activated: yes, all 5 final strategies
- implementation_bug_found: no
- implementation_fix_made: no
- initial_issue: first micro binary/runtime binding failed with CUDA driver version insufficient; fixed wrapper/build path by dynamic cudart rebuild
