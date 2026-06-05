# W24C Postcheck

start_ts=1780652070
end_ts=1780653625
elapsed_sec=1555
start_iso=2026-06-05T17:34:30+08:00
end_iso=2026-06-05T18:00:25+08:00
branch=hrl/paper/mascar-repro-v0
head=aa7a228
postcheck_method=start_ts=$(date +%s) / end_ts=$(date +%s)

analysis_inputs=w24_latest_results.csv;w24_latest_run_manifest.csv;w24_tableiii_manifest_all.csv;w24_tableiii_phase_mapping_used.csv;w24_tableiii_config_matrix.csv
refreshed_result_rows=120
coverage_row_count=30
valid_speedup_row_count=15
actual_result_rows=60
actual_classification_counts={'completed_no_explicit_pass': 44, 'completed_explicit_pass': 16}
M2_active_rows=4
M3_active_rows=0
M4_active_rows=4
geomean_summary=all:m2_owner_sched=0.999671(15); all:m3_hitonly_nack=0.999671(15); all:m4_reexec_load=0.999729(15); memory:m2_owner_sched=0.999510(8); memory:m3_hitonly_nack=0.999510(8); memory:m4_reexec_load=0.999564(8); compute:m2_owner_sched=0.999855(7); compute:m3_hitonly_nack=0.999855(7); compute:m4_reexec_load=0.999917(7)
warnings=current_branch_current_config_not_paper_exact;inferred_order_not_exact;stats_only_not_correctness_pass;energy_not_run
