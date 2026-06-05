start_ts=1780648940
end_ts=1780650135
elapsed_sec=1195
start_iso=2026-06-05T16:42:20+08:00
end_iso=2026-06-05T17:02:15+08:00
branch=hrl/paper/mascar-repro-v0
head=19682d6
postcheck_method=start_ts=$(date +%s) / end_ts=$(date +%s)


## W22 validation
candidate_smoke_rows=2
dryrun_rows=13
smoke_rows=13
smoke_ready_count=13
all_smoke_has_stats=True
promoted_workloads=0
blocker_counts={'no_gpgpusim_stats_from_candidate_command': 2, 'missing external data or validated args': 8}
