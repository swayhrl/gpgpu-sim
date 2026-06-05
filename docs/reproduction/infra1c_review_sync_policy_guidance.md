# INFRA-1C Guidance: Review, Sync Policy, and Handoff

## Goal

Create final extraction summary and review handoff for both infra branches.

Branches:

1. hrl/infra/paper-repro-framework-v0
2. hrl/infra/paper-repro-framework-trace-v0

## Required summary

Create under /workspace/tmp and in the infra branches if appropriate:

  INFRA_EXTRACTION_SUMMARY.md

It must include:

- baseline used
- source branch used
- base infra local commit hash
- trace infra local commit hash
- extracted components
- excluded components
- validation status
- known caveats
- push commands for user review
- sync-back policy to Mascar

## Sync-back policy

The user wants future infra fixes cherry-picked back to Mascar.

Policy:

- Do not rebase hrl/paper/mascar-repro-v0.
- Cherry-pick infra fixes into Mascar only after review.
- Keep sync commits small and explicit.
- Never cherry-pick clean-baseline branch resets into Mascar.
- Never cherry-pick commits that remove Mascar experiment outputs unless explicitly intended.

Example:

  git checkout hrl/paper/mascar-repro-v0
  git cherry-pick <infra_fix_commit>
  git push origin hrl/paper/mascar-repro-v0

## Final combined review pack

Create:

  /workspace/tmp/infra_paper_repro_framework_combined_review_pack_YYYYMMDD_HHMMSS.tar.gz

Include:
- INFRA-1A review pack or files
- INFRA-1B review pack or files
- extraction summary
- branch commit hashes
- validation outputs
- commands to push branches and tags after review

## Final report to GPT

Report only:

1. base infra branch name and local commit hash
2. trace infra branch name and local commit hash
3. baseline ref used
4. source branch used
5. review pack paths
6. validation status
7. whether any Mascar-specific content was detected
8. push/tag commands for after review
