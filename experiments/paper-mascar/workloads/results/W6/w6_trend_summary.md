# W6 Trend Summary

W6 found no activation-ready workload under the current W5C inputs. No M2/M3/M4 active counters were triggered in the ready workload sweep, so the cycle ratios below are smoke-level comparability checks, not Mascar mechanism benefit claims.

## Run Coverage

- Table III rows: 30
- Ready candidate workloads swept: 6
- Configs per ready workload: 4
- Result rows: 24
- Planned rows: 24

## Status Counts

- completed_explicit_pass: 4
- completed_stats_found: 20

## Mechanism Activation

- M2 active workload count: 0
- M3 active workload count: 0
- M4 active workload count: 0
- Any M2/M3/M4 active workload count: 0
- Conclusion: no activation-ready workload found yet; no M2-M4 activation under current inputs.

## Cycle Ratio Geomeans

These ratios are recorded for debugging only because active mechanism counters are zero.

| config | type | valid pairs | explicit pass pairs | active workloads | cycle speedup geomean | interpretation |
| --- | --- | ---: | ---: | ---: | ---: | --- |
| m2_owner_sched | M | 2 | 1 | 0 | 1.000000 | no_activation_no_mechanism_trend |
| m2_owner_sched | C | 4 | 0 | 0 | 1.000000 | no_activation_no_mechanism_trend |
| m2_owner_sched | all_ready | 6 | 1 | 0 | 1.000000 | no_activation_no_mechanism_trend |
| m3_hitonly_nack | M | 2 | 1 | 0 | 1.000000 | no_activation_no_mechanism_trend |
| m3_hitonly_nack | C | 4 | 0 | 0 | 1.000000 | no_activation_no_mechanism_trend |
| m3_hitonly_nack | all_ready | 6 | 1 | 0 | 1.000000 | no_activation_no_mechanism_trend |
| m4_reexec_load | M | 2 | 1 | 0 | 1.000000 | no_activation_no_mechanism_trend |
| m4_reexec_load | C | 4 | 0 | 0 | 1.000000 | no_activation_no_mechanism_trend |
| m4_reexec_load | all_ready | 6 | 1 | 0 | 1.000000 | no_activation_no_mechanism_trend |

## Correctness Caveat

Only rows with `completed_explicit_pass` have an explicit pass signal. `completed_stats_found` rows are simulator-completed smoke rows with stats, not correctness-pass evidence.
