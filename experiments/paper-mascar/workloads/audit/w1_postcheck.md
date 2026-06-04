# W1 Postcheck

start_iso: 2026-06-04T20:00:29+08:00
end_iso: 2026-06-04T20:07:49+08:00
start_ts_cmd: start_ts=$(date +%s)
end_ts_cmd: end_ts=$(date +%s)
elapsed_sec: 440

## Checks

- canonical_manifest_rows: 30
- audited_manifest_rows: 30
- availability_summary_rows: 30
- audit_help: pass
- audit_run: pass
- py_compile: pass

## Outputs

- experiments/paper-mascar/workloads/mascar_table_iii_workload_manifest.csv
- experiments/paper-mascar/workloads/mascar_table_iii_workload_manifest_audited.csv
- experiments/paper-mascar/workloads/mascar_workload_availability_summary.csv
- experiments/paper-mascar/workloads/audit/workload_candidate_paths.txt
- experiments/paper-mascar/workloads/audit/workload_availability_summary.md
- docs/papers/mascar_w1_workload_inventory_report.md
