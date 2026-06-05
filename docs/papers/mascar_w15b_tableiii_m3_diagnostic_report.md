# Mascar W15B Table III M3 Diagnostic Report

## Scope

W15B ran the W15A diagnostic counters on the required Table III-ready workloads: `spmv`, `mri_q`, `pathfinder`, `bp_2`, `srad_1`, `srad_2`, and `bp_1`.

## Configs

- `m4_reexec_load`: reference active M4 config.
- `m3diag_on`: normal diagnostic config.
- `m3diag_forced_mp_on`: forced-MP diagnostic-only config with increased L1 saturation margin/window.

## Result summary

The matrix produced 21 rows. Normal `m3diag_on` completed for all 7 workloads but did not activate active M3 hit-only access. Forced-MP completed for all 7 workloads and activated active M3 hit-only/NACK on `srad_1` and `srad_2`.

## Key evidence

- `srad_1`, `m3diag_forced_mp_on`: `m3diag_nonowner_load_candidate=176`, `m3diag_scheduler_allow_nonowner_load=124`, `m3diag_hitonly_probe_called=52`, `m3_active=1`.
- `srad_2`, `m3diag_forced_mp_on`: same counter pattern as `srad_1`.
- `m3diag_skip_m2_blocked_before_lsu=0` across the active forced-MP rows.
- Normal diagnostic rows are dominated by `skip_not_mp`, `skip_no_owner`, `skip_owner_warp`, or `skip_not_load` rather than scheduler-block-before-LSU.

## Diagnosis

M3 implementation is reachable. The primary Table III blocker is that normal tiny inputs rarely align MP mode, a valid owner, and non-owner load reuse. Forced-MP shows the M3 path can issue non-owner hit-only attempts, so the current evidence does not support a scheduler-to-LSU implementation break.

## Output files

- `experiments/paper-mascar/workloads/results/W15B/w15b_latest_results.csv`
- `experiments/paper-mascar/workloads/results/W15B/w15b_latest_summary.md`
- `experiments/paper-mascar/workloads/results/W15B/w15b_latest_status_matrix.csv`
- `experiments/paper-mascar/workloads/results/W15B/w15b_latest_run_manifest.csv`
- `experiments/paper-mascar/workloads/matrix/W15B/w15b_m3_diagnostic_summary.csv`
