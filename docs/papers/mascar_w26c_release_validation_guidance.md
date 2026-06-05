# Mascar W26C Guidance: Framework Release Validation

## Stage position

This is W26C.

W26B created docs/templates. W26C validates the released framework with lightweight tests.

## Goal

Run non-destructive validation proving framework tools still work after documentation/template closeout.

## Required validation tasks

1. Syntax check shell tools:
   - experiments/common/gpgpusim_matrix/run_gpgpusim_matrix.sh
   - experiments/suites/common/run_full_suite_matrix.sh
   - any tools/paper_repro scripts if created

2. Compile Python tools:
   - experiments/common/gpgpusim_matrix/collect_gpgpusim_stats.py
   - experiments/common/gpgpusim_matrix/collect_kernel_trace.py
   - experiments/suites/common/collect_full_suite_results.py
   - experiments/suites/common/normalize_w20_suite_manifests.py
   - W25 analysis scripts if present

3. Validate templates:
   - CSV headers readable by Python csv.DictReader
   - required columns present
   - no empty header fields

4. Run tiny dry-run validation:
   - use one known ready workload if available
   - DRY_RUN_ONLY=1 only
   - no long actual run required

5. Validate review-pack creation helper if one exists.

## Required outputs

Create:

- experiments/framework_release/W26/w26_validation_results.csv
- experiments/framework_release/W26/w26_template_validation.csv
- experiments/framework_release/W26/w26_tool_validation.csv
- docs/papers/mascar_w26c_framework_validation_report.md

## validation_results.csv columns

- check_id
- component
- command
- exit_code
- status
- notes

## W26C report

docs/papers/mascar_w26c_framework_validation_report.md sections:

1. Goal
2. Tools validated
3. Templates validated
4. Dry-run validation
5. Failures and fixes
6. Release readiness

## Stop conditions

Stop only if:
1. core runner or collector is broken and cannot be fixed
2. template validation fails broadly
3. repository state becomes unsafe

Do not run full benchmark suites in W26C.
