# Mascar W26A Guidance: Paper-Reproduction Framework Release Audit

## Stage position

This is W26A.

W25 integrated energy. W26 now releases the reusable paper-reproduction framework.

W26 should not focus on running new experiments. It should package, document, template, validate, and close out the framework.

## Goal

Audit all framework components produced across W1-W25 and decide what is ready for release.

Components:
- common matrix runner
- common stats collector
- kernel trace collector
- full-suite runner/collector
- workload manifests
- config matrices
- wrapper conventions
- status taxonomy
- postcheck conventions
- raw log policy
- review pack policy
- energy pipeline
- kernel trace / phase mapping pipeline

## Inputs to audit

Read if present:

- experiments/common/gpgpusim_matrix/run_gpgpusim_matrix.sh
- experiments/common/gpgpusim_matrix/collect_gpgpusim_stats.py
- experiments/common/gpgpusim_matrix/collect_kernel_trace.py
- experiments/suites/common/run_full_suite_matrix.sh
- experiments/suites/common/collect_full_suite_results.py
- experiments/suites/common/full_suite_manifest.csv
- experiments/suites/common/full_suite_command_manifest.csv
- experiments/suites/common/suite_status_taxonomy.md
- experiments/paper-mascar/workloads/mascar_table_iii_workload_manifest.csv
- experiments/paper-mascar/workloads/mascar_table_iii_command_manifest.csv
- docs/reproduction_workload_framework.md
- experiments/common/README.md
- experiments/suites/README.md
- tools/paper_repro/README.md

## Required outputs

Create:

- experiments/framework_release/W26/
- experiments/framework_release/W26/w26_framework_component_audit.csv
- experiments/framework_release/W26/w26_framework_gap_list.csv
- docs/papers/mascar_w26a_framework_release_audit.md

## component audit columns

- component
- path
- exists
- executable_or_importable
- owner_area
- purpose
- current_status
- required_for_release
- validation_command
- notes

current_status values:
- ready
- usable_with_caveat
- missing
- needs_doc
- needs_template
- deprecated
- unknown

## gap list columns

- priority
- component
- gap
- fix_plan
- required_before_release
- notes

## W26A report

docs/papers/mascar_w26a_framework_release_audit.md sections:

1. Goal
2. Components audited
3. Ready components
4. Usable with caveat
5. Gaps
6. Required fixes for W26B/W26C
7. Release scope

## Validation

Run:
- bash -n for shell tools that exist
- python3 -m py_compile for Python tools that exist
- do not run long experiments
- git diff --check

## Stop conditions

Stop only if:
1. core framework files are missing and cannot be located
2. audit script cannot run
3. repository state becomes unsafe

Do not stop because documentation gaps exist. W26B will fix them.
