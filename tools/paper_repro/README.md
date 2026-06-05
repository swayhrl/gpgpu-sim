# Paper Reproduction Workflow

Recommended workflow for a new paper:

1. Create a workload manifest.
2. Create a config matrix.
3. Run dry-run all rows.
4. Run smoke on ready rows only.
5. Collect stats.
6. Write postcheck with start/end elapsed seconds.
7. Package a review pack under `/workspace/tmp`.

Keep placeholders and blockers. Do not actual-run unavailable rows by default. Treat `completed_no_explicit_pass` as simulator completion evidence, not correctness pass.
