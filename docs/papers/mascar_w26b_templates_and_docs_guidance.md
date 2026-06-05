# Mascar W26B Guidance: Templates, Documentation, and User Recipes

## Stage position

This is W26B.

W26A audited framework components. W26B writes release-quality templates and user-facing documentation.

## Goal

Make the framework reusable for new papers and user experiments.

## Required directories

Create or update:

- docs/reproduction/
- experiments/templates/
- tools/paper_repro/

## Required templates

Create:

- experiments/templates/workload_manifest_template.csv
- experiments/templates/config_matrix_template.csv
- experiments/templates/result_manifest_template.csv
- experiments/templates/postcheck_template.md
- experiments/templates/review_pack_manifest_template.csv
- experiments/templates/paper_repro_report_template.md
- experiments/templates/status_taxonomy_template.csv

## workload_manifest_template.csv columns

- workload_id
- paper_id
- suite
- app
- benchmark
- input_name
- wrapper_path
- working_dir
- command
- availability_status
- wrapper_status
- data_status
- correctness_status
- default_timeout_sec
- notes

## config_matrix_template.csv columns

- config_id
- config_path
- config_role
- enabled
- expected_behavior
- notes

## Documentation

Create or update:

- docs/reproduction_workload_framework.md
- docs/reproduction/add_new_paper.md
- docs/reproduction/add_new_workload.md
- docs/reproduction/add_new_config_family.md
- docs/reproduction/run_smoke.md
- docs/reproduction/run_full_sweep.md
- docs/reproduction/collect_stats.md
- docs/reproduction/energy_pipeline.md
- docs/reproduction/kernel_trace_phase_mapping.md
- docs/reproduction/raw_log_policy.md
- docs/reproduction/status_taxonomy.md
- docs/reproduction/nightly_codex_workflow.md

Update or create:

- experiments/common/README.md
- experiments/suites/README.md
- experiments/suites/common/README.md
- tools/paper_repro/README.md

## Required content

The docs must explain:

1. How to add a new paper.
2. How to create a workload manifest.
3. How to create config matrix.
4. How to write wrappers.
5. How to run dry-run all.
6. How to run smoke ready.
7. How to run full sweep.
8. How to collect stats.
9. How to interpret status.
10. How to handle raw logs.
11. How to use energy configs.
12. How to enable kernel trace.
13. How to write postcheck.
14. How to create review pack.
15. What not to claim without paper-comparable setup.

## Required report

Create:

- docs/papers/mascar_w26b_templates_documentation_report.md

Include:
1. Goal
2. Templates created
3. Documentation created
4. Reuse workflow
5. Known limitations
6. Validation performed

## Validation

Run:
- Check templates have headers.
- Check README/docs exist.
- bash -n shell snippets if any script created.
- git diff --check.

## Stop conditions

Stop only if:
1. docs/templates cannot be written
2. repository state becomes unsafe

Do not stop because some framework components have caveats. Document them.
