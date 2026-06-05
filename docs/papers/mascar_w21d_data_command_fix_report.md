# Mascar W21D Missing Data / Command Normalization Report

## Scope
W21D targeted missing data, command, wrapper, and smoke blockers.

## Strategies attempted
- Probe suite-level and wrapper-level data paths.
- Probe candidate run commands for existing binaries.
- Keep placeholder/unavailable behavior as exit 77 when actual command is unavailable.

## Results
`data_availability_results_w21d.csv` rows: 56.
`wrapper_command_normalization_results.csv` rows: 56.
Command candidate pass rows: 1.

## Updated blocker classification
Status counts: {'still_unavailable': 8, 'binary_or_dependency_probe_passed_command_unverified': 7, 'ready_candidate_needs_collector_smoke': 1, 'binary_recovered_command_data_unverified': 9, 'parboil_build_unresolved': 3}.
Blocker counts: {'command/data not normalized in workload manifest': 2, 'no verified executable binary': 6, 'command_or_data_unverified': 7, 'not_promoted_without_full_gpgpusim_stats_smoke': 1, 'missing_or_unverified_data_and_run_command': 9, 'python2_driver_or_cuda11_source_compat_or_missing_data': 3}.

## Newly ready decision
No workload is promoted to ready in W21, because none of the previously blocked workloads completed a full normalized GPGPU-Sim collector smoke with explicit pass evidence. There is 1 ready candidate requiring full collector smoke and 9 Parboil binary-recovered rows requiring data/command normalization.

## Validation smoke
W21 ran dry-run/sample smoke over current-ready rows only. Blockers/placeholders were not actual-run by default.
