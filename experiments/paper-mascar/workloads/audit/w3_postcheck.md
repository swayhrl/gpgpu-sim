# W3 Postcheck

start_iso: 2026-06-04T20:24:11+08:00
end_iso: 2026-06-04T20:36:47+08:00
start_ts_cmd: start_ts=$(date +%s)
end_ts_cmd: end_ts=$(date +%s)
elapsed_sec: 756
branch: hrl/paper/mascar-repro-v0
head: 27f60ae920270b3ce8be11215d9658b1681ae2df

## Commands Run

- bash -n experiments/common/gpgpusim_matrix/run_gpgpusim_matrix.sh
- python3 -m py_compile experiments/common/gpgpusim_matrix/collect_gpgpusim_stats.py
- bash -n experiments/paper-mascar/workloads/matrix/run_mascar_table_iii_matrix.sh
- python3 -m py_compile experiments/paper-mascar/workloads/matrix/collect_mascar_table_iii_matrix.py
- DRY_RUN_ONLY=1 OUTDIR=experiments/paper-mascar/workloads/results/w3_dryrun_matrix bash experiments/paper-mascar/workloads/matrix/run_mascar_table_iii_matrix.sh
- python3 experiments/common/gpgpusim_matrix/collect_gpgpusim_stats.py experiments/paper-mascar/workloads/results/w3_dryrun_matrix
- python3 experiments/paper-mascar/workloads/matrix/collect_mascar_table_iii_matrix.py experiments/paper-mascar/workloads/results/w3_dryrun_matrix

## Results

- dry_run_matrix_rows: 60
- dry_run_collection_status: pass
- common_runner_status: pass
- common_collector_status: pass
- mascar_runner_status: pass
- mascar_collector_status: pass
