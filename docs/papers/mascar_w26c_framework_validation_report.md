# Mascar W26C Framework Validation Report

Validation checks completed: 14/14 passed.

Validation covered `git diff --check`, release build after W25 actual energy runs, key Python compilation, key shell syntax, reusable template CSV headers, and a tiny W25 dry-run under `/workspace/tmp`.

Detailed outputs:

- `experiments/framework_release/W26/w26_validation_results.csv`
- `experiments/framework_release/W26/w26_template_validation.csv`
- `experiments/framework_release/W26/w26_tool_validation.csv`

Current-simulator caveat remains: W25 did not expose true power/energy fields in actual run logs, so no paper GPUWattch/GTX480 energy-saving claim is made.
