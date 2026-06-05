# Reproduction Workload Framework

## Purpose
Provide a reusable workflow for workload manifests, config matrices, runners, collectors, validation, and review packs.

## Repository layout
- `experiments/common`: shared runner and collector infrastructure.
- `experiments/suites`: Rodinia/Parboil full-suite framework.
- `experiments/paper-mascar`: Mascar-specific reproduction artifacts.
- `tools/paper_repro`: high-level workflow notes.

## Workload manifest format
Use canonical columns from `experiments/suites/common/suite_manifest_schema.md`.

## Config matrix format
Config matrices should provide `config_id`, `config_path`, `config_role`, `enabled`, and notes.

## Wrapper convention
Wrappers must support dry-run and should avoid actual-running placeholders by default.

## Runner usage
Use `run_full_suite_matrix.sh` for suites or the common matrix runner for custom paper manifests.

## Collector usage
Use `collect_full_suite_results.py` for suite runs and `collect_gpgpusim_stats.py` for generic matrix runs.

## Status taxonomy
Use `suite_status_taxonomy.csv`. Keep missing, failed, unsupported, and placeholder rows explicit.

## Dry-run workflow
Normalize the manifest, run dry-run all, and check dry-run row count equals manifest row count.

## Smoke workflow
Run actual smoke on ready rows only, with timeout.

## Full-sweep workflow
After smoke passes, enable additional configs and run a larger matrix deliberately.

## Raw log policy
Archive large raw logs to `/workspace/tmp`; keep stable CSV and summary outputs in repo.

## Adding a new paper
Create workload and config manifests, dry-run, smoke, collect, write reports, and package a review pack.

## Adding a new workload
Add source, binary, data, and command evidence; dry-run it; smoke it; then update status.

## Adding a new config family
Create disabled-by-default config rows, smoke a sample, then enable deliberately.

## Handling failures and timeouts
Classify failures and continue. Do not collapse all failures into unknown.

## Energy fields and power trend integration
W16 power fields are parsed by the common collector when present.

## Kernel trace integration
W18 kernel trace is a separate default-off facility; do not overwrite proposed phase mappings without review.

## Known limitations
W20 does not fix blocked builds or legacy CUDA issues. That is reserved for a later build-recovery round.
