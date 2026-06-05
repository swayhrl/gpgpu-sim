# W15B Postcheck

- start_iso: 2026-06-05T10:04:11+08:00
- end_iso: 2026-06-05T10:24:31+08:00
- elapsed_sec: 1220
- build_result: W15A build passed before W15B.
- dry_run: experiments/paper-mascar/workloads/results/W15B/w15b_dryrun
- actual_runs: experiments/paper-mascar/workloads/results/W15B/w15b_actual and w15b_forced_actual
- rows: 21
- m3_activated_active_hitonly: yes, srad_1 and srad_2 under m3diag_forced_mp_on
- primary_w15b_blocker_for_normal_diag: not_mp / no_owner / no_nonowner_load alignment on tiny inputs
- implementation_bug_evidence: none; skip_m2_blocked_before_lsu remained 0 in active forced-MP rows
