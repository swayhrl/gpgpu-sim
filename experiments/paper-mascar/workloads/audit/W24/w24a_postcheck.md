# W24A Postcheck

start_ts=1780652070
end_ts=1780652156
elapsed_sec=86
start_iso=2026-06-05T17:34:30+08:00
end_iso=2026-06-05T17:35:56+08:00
branch=hrl/paper/mascar-repro-v0
head=aa7a228
postcheck_method=start_ts=$(date +%s) / end_ts=$(date +%s)

all_manifest_rows=30
ready_rows=15
unavailable_rows=15
run_plan_rows=60
config_enabled_core=4
validation=planner_py_compile_pass;planner_run_pass;runner_bash_n_pass
