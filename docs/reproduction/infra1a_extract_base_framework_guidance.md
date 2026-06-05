# INFRA-1A Guidance: Extract Reusable Paper Reproduction Framework

## Stage position

This is INFRA-1A.

Goal:
Create a clean reusable paper reproduction infrastructure branch from a clean GPGPU-Sim baseline.

Target branch:
  hrl/infra/paper-repro-framework-v0

Baseline:
  baseline-a4ce3fe if available.
  If not available, locate the cleanest baseline candidate and stop with a report.

Source branch:
  hrl/paper/mascar-repro-v0

Important:
This branch must not include Mascar mechanism code, Mascar configs, Mascar experiment results, paper-specific reports, review packs, or raw logs.

## Core principle

Do not cherry-pick whole Mascar commits.

Use allowlist extraction:
- copy only reusable infrastructure paths
- remove or rename local-result artifacts
- document exactly what was extracted
- document exactly what was excluded

## Hard constraints

1. Do not fetch upstream.
2. Do not modify hrl/paper/mascar-repro-v0 in this task.
3. Do not force-reset any existing infra branch.
4. Do not include Mascar M1-M4 mechanism code.
5. Do not include Mascar configs with gpgpu_mascar_* options.
6. Do not include Mascar experiment results or paper-specific reports.
7. Do not include raw logs, review packs, W* result directories, or large artifacts.
8. Do not use git add . or git add -A.
9. Local commits are allowed and expected for new infra branches.
10. Do not push branches in INFRA-1. Report local commit hashes for review.
11. If the target branch already exists, do not overwrite it. Stop and report unless it is clearly safe to continue.

## Baseline discovery

Run checks:

  git rev-parse --verify baseline-a4ce3fe
  git rev-parse --verify refs/tags/baseline-a4ce3fe
  git rev-parse --verify a4ce3fe

If all fail, inspect:

  git tag -l '*baseline*'
  git branch -a | grep -i baseline
  git log --oneline --decorate --all | grep -i baseline | head -n 50

If no clean baseline can be identified, stop and create:
  /workspace/tmp/infra1_baseline_missing_report.md

Do not guess.

## Branch creation

If baseline exists:

  git checkout <baseline_ref>
  git checkout -b hrl/infra/paper-repro-framework-v0

If the branch already exists locally or on origin:
- do not use -B
- stop and report branch existence
- do not overwrite user work

## Allowed extraction paths

From source branch hrl/paper/mascar-repro-v0, extract only these reusable paths if they exist:

  experiments/common/gpgpusim_matrix/
  experiments/common/README.md
  experiments/templates/
  docs/reproduction/
  docs/reproduction_workload_framework.md
  tools/paper_repro/README.md
  experiments/suites/common/
  experiments/suites/README.md

Suggested command pattern:

  git checkout hrl/paper/mascar-repro-v0 -- <path>

If a path does not exist, record it and continue.

## Required cleanup after extraction

Remove or avoid committing:

  experiments/paper-mascar/
  docs/papers/mascar_w*.md
  configs/hrl-repro/SM7_QV100_mascar_*
  review_packs/
  *_review_pack_*
  experiments/suites/results/
  experiments/suites/audit/
  __pycache__/
  *.pyc
  raw logs
  *.tar.gz
  *.zip

Use explicit rm commands. Do not use broad dangerous deletes.

## Local-suite manifest policy

If extracted suite files include local machine manifests, keep them only as local examples.

Rename if present:

  experiments/suites/common/full_suite_manifest.csv
    -> experiments/suites/common/full_suite_manifest.local.example.csv

  experiments/suites/common/full_suite_command_manifest.csv
    -> experiments/suites/common/full_suite_command_manifest.local.example.csv

  experiments/suites/common/full_suite_ready_manifest.csv
    -> experiments/suites/common/full_suite_ready_manifest.local.example.csv

  experiments/suites/common/full_suite_blocker_manifest.csv
    -> experiments/suites/common/full_suite_blocker_manifest.local.example.csv

Keep schema/taxonomy/scripts as canonical framework files.

## Must-keep infrastructure capabilities

The base infra branch should include:

1. Generic matrix runner:
   experiments/common/gpgpusim_matrix/run_gpgpusim_matrix.sh

2. Generic stats collector:
   experiments/common/gpgpusim_matrix/collect_gpgpusim_stats.py

3. Kernel trace parser:
   experiments/common/gpgpusim_matrix/collect_kernel_trace.py

4. Energy artifact recovery support:
   - runner copies power artifacts into run_dir/power_artifacts
   - collector bounded-recursively scans run_dir and power_artifacts
   - collector parses kernel_avg_power / gpu_tot_avg_power if present

5. Templates:
   experiments/templates/

6. Reproduction docs:
   docs/reproduction/
   docs/reproduction_workload_framework.md

7. Suite framework:
   experiments/suites/common/
   experiments/suites/README.md

## Mascar-specific parser policy

It is acceptable for collect_gpgpusim_stats.py to contain optional parser fields for:
- paper_mascar_*
- paper_mascar_m3diag_*

But docs must clearly say:
- these fields are optional
- absent fields do not affect other papers
- this branch does not implement Mascar

## Required new documentation

Create:

  docs/reproduction/INFRA_BASELINE.md
  docs/reproduction/BASELINE_POLICY.md
  docs/reproduction/BRANCHING_STRATEGY.md
  docs/reproduction/WHAT_WAS_EXTRACTED.md
  docs/reproduction/WHAT_IS_NOT_INCLUDED.md
  docs/reproduction/SYNC_BACK_TO_PAPER_BRANCHES.md
  docs/reproduction/INFRA_VALIDATION.md

## INFRA_BASELINE.md must include

- baseline ref used
- source branch used for extraction
- local commit hash
- statement that simulator mechanisms are not changed
- statement that framework files are for reproducibility orchestration only

## WHAT_WAS_EXTRACTED.md must include

- paths extracted
- why each path is reusable
- whether it is pure script/doc/template or local snapshot
- any renamed local manifests

## WHAT_IS_NOT_INCLUDED.md must explicitly list

Not included:
- Mascar M1-M4 mechanism
- Mascar configs
- Mascar diagnostic configs
- Mascar experiment results
- Mascar reports
- raw logs
- review packs
- paper-specific data outputs

## SYNC_BACK_TO_PAPER_BRANCHES.md must include

Policy:
- future infra fixes may be cherry-picked back to hrl/paper/mascar-repro-v0
- do not rebase Mascar branch
- sync fixes one commit at a time
- each sync commit must be reviewed
- Mascar paper results are not copied into infra branch

Example command:

  git checkout hrl/paper/mascar-repro-v0
  git cherry-pick <infra_fix_commit>

## Validation

Run:

  git diff --check
  bash -n experiments/common/gpgpusim_matrix/run_gpgpusim_matrix.sh
  python3 -m py_compile experiments/common/gpgpusim_matrix/collect_gpgpusim_stats.py
  python3 -m py_compile experiments/common/gpgpusim_matrix/collect_kernel_trace.py

If these files exist, also run:

  bash -n experiments/suites/common/run_full_suite_matrix.sh
  python3 -m py_compile experiments/suites/common/collect_full_suite_results.py
  python3 -m py_compile experiments/suites/common/normalize_w20_suite_manifests.py

Check there are no simulator source changes in base infra:

  git diff --name-only <baseline_ref>..HEAD -- src configs | tee /tmp/infra_src_config_diff.txt

This should be empty or contain only non-source template/example files. If src/gpgpu-sim appears, stop and remove those changes from base infra.

Check no Mascar configs:

  find configs -type f 2>/dev/null | grep -i mascar && stop/report

Check no paper-mascar results:

  find experiments -path '*paper-mascar*' -print && stop/report

## Postcheck

Create:

  docs/reproduction/INFRA1A_POSTCHECK.md

Include:
- start_iso
- end_iso
- elapsed_sec
- baseline_ref
- source_branch
- new_branch
- local_commit_hash
- extracted paths
- excluded paths
- validation commands and results
- warnings

## Review pack

Create:

  /workspace/tmp/infra1a_paper_repro_framework_review_pack_YYYYMMDD_HHMMSS.tar.gz

Include:
- docs/reproduction/
- experiments/common/
- experiments/templates/
- experiments/suites/common/
- experiments/suites/README.md
- tools/paper_repro/README.md
- full diff against baseline
- postcheck

## Commit

If validation passes:

  git add <explicit extracted files and docs>
  git commit -m "infra: extract reusable paper reproduction framework"

Do not push.

Report:
- branch name
- local commit hash
- review pack path
