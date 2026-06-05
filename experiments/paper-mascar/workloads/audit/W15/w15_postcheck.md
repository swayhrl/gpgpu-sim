# W15 Postcheck

- start_iso: 2026-06-05T10:04:11+08:00
- end_iso: 2026-06-05T10:36:29+08:00
- elapsed_sec: 1955
- branch: hrl/paper/mascar-repro-v0
- HEAD: 73f31df0c30b549dbeb042f03980144c2e87c5b9
- build_result: pass; final command was `source setup_environment release && make -j2`
- static_validation: pass; `git diff --check`, collector py_compile, and bash -n checks passed
- configs_created: SM7_QV100_mascar_m3diag_on, SM7_QV100_mascar_m3diag_forced_mp_on
- runs_attempted: W15B 21 rows; W15C 5 final micro strategies plus earlier oversized/runtime-binding attempts documented
- W15B_result: M3 active hit-only activated for srad_1 and srad_2 under m3diag_forced_mp_on
- W15C_result: M3 active hit-only activated for all 5 final micro strategies
- M3_activated: yes
- M3_implementation_bug_found: no
- mechanism_code_changed: yes, W15A diagnostic-only counters/traces gated by gpgpu_mascar_enable_m3_diagnostic
- mechanism_bug_fix_made: no
- raw_logs_archive_path: /workspace/tmp/mascar_w15_m3_raw_logs_20260605_103628.tar.gz
- review_pack_path: /workspace/tmp/mascar_w15_m3_diagnostic_review_pack_20260605_103646.tar.gz
- warnings: forced-MP is diagnostic only; W15C large initial parameters were reduced to smoke-sized variants after timeout; micro build required dynamic cudart for GPGPU-Sim interception

## git status --short at postcheck

```
 M experiments/common/gpgpusim_matrix/collect_gpgpusim_stats.py
 M src/gpgpu-sim/gpu-sim.cc
 M src/gpgpu-sim/shader.cc
 M src/gpgpu-sim/shader.h
?? configs/hrl-repro/SM7_QV100_mascar_m3diag_forced_mp_on/
?? configs/hrl-repro/SM7_QV100_mascar_m3diag_on/
?? docs/papers/mascar_w15_m3_diagnosis_report.md
?? docs/papers/mascar_w15a_m3_static_diagnostic.md
?? docs/papers/mascar_w15b_tableiii_m3_diagnostic_report.md
?? docs/papers/mascar_w15c_m3_microbenchmark_report.md
?? experiments/paper-mascar/workloads/audit/W15/
?? experiments/paper-mascar/workloads/matrix/W15/
?? experiments/paper-mascar/workloads/matrix/W15B/
?? experiments/paper-mascar/workloads/matrix/W15C/
?? experiments/paper-mascar/workloads/micro/
?? experiments/paper-mascar/workloads/results/W15B/
?? experiments/paper-mascar/workloads/results/W15C/
```

## Raw Logs Archive

Large W15 per-run raw logs were not committed to git. They were archived locally at:

```text
/workspace/tmp/mascar_w15_raw_runs_20260605_110436.tar.gz
```
