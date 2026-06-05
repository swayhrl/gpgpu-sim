# W25A Postcheck

start_ts=1780654568
end_ts=1780654631
elapsed_sec=63
start_iso=2026-06-05T18:16:08+08:00
end_iso=2026-06-05T18:17:11+08:00
branch=hrl/paper/mascar-repro-v0
head=a1eaff1
postcheck_method=start_ts=$(date +%s) / end_ts=$(date +%s)
input_status=experiments/paper-mascar/energy/W25/audit/w25a_input_status.csv
selected_workload_count=6
enabled_config_count=2
run_plan_row_count=12
warnings=current_simulator_energy_trend_only;not_paper_gpuwattch_gtx480
