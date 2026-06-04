# W2 Postcheck

start_iso: 2026-06-04T20:00:29+08:00
end_iso: 2026-06-04T20:07:49+08:00
start_ts_cmd: start_ts=$(date +%s)
end_ts_cmd: end_ts=$(date +%s)
elapsed_sec: 440

## Checks

- command_manifest_rows: 30
- wrapper_count: 30
- ready_wrappers: 6
- placeholder_wrappers: 24
- bash_n_build_helper: pass
- bash_n_smoke_helper: pass
- bash_n_all_wrappers: pass
- dry_run_smoke_all_30: pass
- actual_smoke_attempted: stencil only
- actual_smoke_status: pass

## Smoke Evidence

- dry_run_results: experiments/paper-mascar/workloads/audit/w2_final_dryrun_smoke/smoke_results.csv
- actual_stencil_log: experiments/paper-mascar/workloads/audit/w2_actual_smoke/stencil/stencil.log
