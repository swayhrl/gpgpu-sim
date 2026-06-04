start_iso=2026-06-04T18:53:33+08:00
end_iso=2026-06-04T18:59:09+08:00
start_ts=1780570413
end_ts=1780570749
elapsed_sec=336
branch=hrl/paper/mascar-repro-v0
head=2cf8b33 (HEAD -> hrl/paper/mascar-repro-v0, origin/hrl/paper/mascar-repro-v0) docs: add Mascar M5 M6 validation closeout guidance

Validation
- git diff --check: pass
- source setup_environment release && make -j2: pass
- bash -n experiments/paper-mascar/run_m5_focused_validation.sh: pass
- python3 -m py_compile experiments/paper-mascar/collect_m5_results.py: pass
- collector rerun on experiments/paper-mascar/m5_runs/20260604_185613: pass

M5 runtime status
- Actual runtime workloads ran: yes
- Workload: rodinia_hotspot
- Focused matrix rows: 6
- Completed rows: 6
- Failed/timeouts: 0

Final report status
- docs/papers/mascar_final_reproduction_report.md updated from actual M5 results
- No paper-comparable speedup or energy claim made

Changed files
 M docs/papers/mascar_final_reproduction_report.md
?? configs/hrl-repro/SM7_QV100_mascar_baseline_off/
?? docs/papers/mascar_closeout_summary.md
?? experiments/paper-mascar/collect_m5_results.py
?? experiments/paper-mascar/m5_config_matrix.csv
?? experiments/paper-mascar/m5_results.csv
?? experiments/paper-mascar/m5_results_summary.md
?? experiments/paper-mascar/m5_runs/
?? experiments/paper-mascar/m5_runtime_logs/
?? experiments/paper-mascar/m5_workload_manifest.csv
?? experiments/paper-mascar/m5_workload_manifest_README.md
?? experiments/paper-mascar/m5a_runtime_env_audit.md
?? experiments/paper-mascar/m5a_runtime_sanity.md
?? experiments/paper-mascar/m6_closeout_manifest.csv
?? experiments/paper-mascar/m6_diff_name_status.txt
?? experiments/paper-mascar/m6_postcheck.md
?? experiments/paper-mascar/m6_symbol_grep.txt
?? experiments/paper-mascar/run_m5_focused_validation.sh

Review pack path
/workspace/tmp/mascar_m5_m6_review_pack_20260604_185922.tar.gz

Warnings
- GPGPU-Sim build still emits existing warnings; no build errors.
- M3 hit-only attempts were zero on the short focused workload.
