# Mascar W26D Guidance: Final Paper-Repro Framework Closeout

## Stage position

This is W26D, final closeout for W25+W26 and the current Mascar reproduction framework phase.

## Goal

Produce a complete release closeout summarizing:
- Mascar mechanism implementation status
- Table III refreshed status
- energy pipeline status
- Rodinia/Parboil full-suite framework status
- reusable paper reproduction framework
- remaining blockers and next steps

## Required final docs

Create:

- docs/papers/mascar_w26_paper_repro_framework_release.md
- docs/papers/mascar_w25_w26_closeout_summary.md

## mascar_w26_paper_repro_framework_release.md sections

1. Executive summary
2. What is released
3. Framework layout
4. Supported workflows
5. Workload manifests
6. Config matrices
7. Runner and collector
8. Energy pipeline
9. Kernel trace pipeline
10. Rodinia/Parboil full-suite status
11. Mascar Table III status
12. Known limitations
13. How to start a new paper reproduction
14. Next recommended work

## mascar_w25_w26_closeout_summary.md sections

1. W25 summary
2. W26 summary
3. Artifacts created
4. Validation results
5. Caveats
6. Raw logs archive policy
7. Review pack path
8. Recommended tags

## Required closeout artifacts

Create:

- experiments/framework_release/W26/w26_release_manifest.csv
- experiments/framework_release/W26/w26_known_limitations.md
- experiments/framework_release/W26/w26_next_steps.md
- experiments/framework_release/W26/w26_postcheck.md
- experiments/framework_release/W26/w26_diff_name_status.txt
- experiments/framework_release/W26/w26_symbol_grep.txt

w26_release_manifest.csv columns:

- artifact
- path
- status
- description

Known limitations must mention:
- current simulator/config differs from paper GPUWattch GTX480
- Table III phase mapping may be inferred, not exact
- some Rodinia/Parboil workloads still blocked
- energy is current-simulator trend, not paper-exact energy
- completed_stats_found is not correctness pass
- raw logs are archived, not committed

## Review pack

Create:

- /workspace/tmp/mascar_w25_w26_framework_release_review_pack_YYYYMMDD_HHMMSS.tar.gz

Include:
- W25 energy files
- W26 framework release files
- docs/reproduction/
- experiments/templates/
- key common runner/collector files
- W26 validation files
- postcheck
- full git diff patch

Do not include huge raw logs.

## Final validation

Run:
- git diff --check
- python3 -m py_compile key Python tools
- bash -n key shell scripts
- Check docs/templates exist
- Check review pack exists
- Check no __pycache__
- Check no large raw runs dirs intended for git

## Final report to GPT

Report only:
1. elapsed_sec
2. review pack path
3. git status --short
4. W25 energy status
5. W26 framework release status
6. validation status
7. ready workload/framework counts
8. files GPT should review
