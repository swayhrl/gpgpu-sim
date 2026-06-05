# Mascar W20D Guidance: Full-Suite Framework Closeout

## Stage position

This is W20D, final closeout for W20.

W20A normalized schema/manifests.
W20B stabilized runner/collector.
W20C validated dry-run and smoke.
W20D writes final framework documentation and review pack.

## Goal

Produce a reusable Rodinia/Parboil full-suite framework for future paper reproduction and architecture experiments.

## Required docs

Create or update:

- docs/papers/mascar_w20_full_suite_framework_closeout.md
- docs/reproduction_workload_framework.md
- experiments/common/README.md
- experiments/suites/README.md
- experiments/suites/common/README.md
- tools/paper_repro/README.md

If directories do not exist, create them.

## docs/reproduction_workload_framework.md requirements

Sections:
1. Purpose
2. Repository layout
3. Workload manifest format
4. Config matrix format
5. Wrapper convention
6. Runner usage
7. Collector usage
8. Status taxonomy
9. Dry-run workflow
10. Smoke workflow
11. Full-sweep workflow
12. Raw log policy
13. Adding a new paper
14. Adding a new workload
15. Adding a new config family
16. Handling failures and timeouts
17. Energy fields and power trend integration
18. Kernel trace integration
19. Known limitations

## experiments/suites/README.md requirements

Sections:
1. Rodinia/Parboil suite inventory
2. Current ready counts
3. How to run dry-run all
4. How to run smoke ready
5. How to inspect blockers
6. How to add or fix wrappers

## tools/paper_repro/README.md requirements

This should be a high-level user guide.

Include:
- recommended workflow for a new paper
- create workload manifest
- create config matrix
- run dry-run
- run smoke
- collect stats
- write postcheck
- package review pack

## Closeout manifests

Create:

- experiments/suites/audit/W20/w20_closeout_manifest.csv
- experiments/suites/audit/W20/w20_diff_name_status.txt
- experiments/suites/audit/W20/w20_symbol_grep.txt
- experiments/suites/audit/W20/w20_postcheck.md

w20_closeout_manifest.csv columns:

- artifact
- path
- status
- description

Include:
- full_suite_manifest.csv
- full_suite_command_manifest.csv
- ready_manifest
- blocker_manifest
- runner
- collector
- dry-run results
- smoke results
- reports
- docs

## Final report

docs/papers/mascar_w20_full_suite_framework_closeout.md must include:

1. Executive summary
2. W20 inputs from W19
3. Normalized manifests
4. Runner and collector
5. Dry-run and smoke validation
6. Current ready baseline
7. Blocker categories
8. How this supports Mascar
9. How this supports future papers
10. Next recommended round W21
11. Limitations

## Review pack

Create:

- /workspace/tmp/mascar_w20_full_suite_framework_review_pack_YYYYMMDD_HHMMSS.tar.gz

Include:
- experiments/suites/common/
- experiments/suites/results/W20/
- experiments/suites/audit/W20/
- experiments/common README if changed
- tools/paper_repro README
- docs/reproduction_workload_framework.md
- docs/papers/mascar_w20*.md
- full git diff patch

Do not include huge raw logs.

## Final validation

Run:
- git diff --check
- bash -n experiments/suites/common/run_full_suite_matrix.sh
- python3 -m py_compile experiments/suites/common/collect_full_suite_results.py
- python3 -m py_compile experiments/suites/common/normalize_w20_suite_manifests.py
- Check W20 postcheck exists.
- Check review pack exists.
- Check no __pycache__.
- Check no raw huge runs dirs intended for git.

## Final report to GPT

Report only:
1. elapsed_sec
2. review pack path
3. git status --short
4. ready workload counts
5. dry-run row counts
6. smoke row counts
7. blocker categories
8. files GPT should review
