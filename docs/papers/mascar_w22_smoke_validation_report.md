# Mascar W22 Smoke Validation Report

## Goal
Advance W21 binary-recovered workloads toward smoke-ready, while preserving placeholder/unavailable rows and validating current smoke-ready rows.

## W22A candidate promotion attempts
Two candidate workloads were attempted in candidate smoke: `rodinia_huffman` and `parboil_simple_mm`.
Both completed with exit code 0 but produced no GPGPU-Sim stats, so neither was promoted.

## W22B final smoke-ready validation
Final smoke-ready workload count: 13.
Dry-run rows: 13.
Actual smoke rows: 13.
Smoke classification: {'completed_stats_found': 11, 'completed_explicit_pass': 2}.
All final smoke rows had `has_stats=1`.

## Promotion record
No W21 binary-recovered workload was promoted to ready in W22. `parboil_simple_mm` remains blocked by no-stats candidate command. Other Parboil recovered binaries remain blocked by missing external data or validated args.

## Blockers and next action
Blocker counts: {'no_gpgpusim_stats_from_candidate_command': 2, 'missing external data or validated args': 8}.
Next action is to derive commands that launch CUDA kernels under GPGPU-Sim for `rodinia_huffman`/`parboil_simple_mm`, and create/import tiny datasets for recovered Parboil workloads.

## Correctness policy
Stats-only completion is baseline characterization evidence, not correctness pass. Only explicit pass rows are counted as explicit correctness pass.
