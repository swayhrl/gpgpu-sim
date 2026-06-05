# Sync Back to Paper Branches

Future infrastructure fixes may be cherry-picked back to paper branches after review.

Policy:

- Do not rebase `hrl/paper/mascar-repro-v0`.
- Sync one infra fix commit at a time.
- Review every sync commit before applying it to a paper branch.
- Never cherry-pick clean-baseline branch resets into paper branches.
- Do not copy paper results into infra branches.

Example:

```bash
git checkout hrl/paper/mascar-repro-v0
git cherry-pick <infra_fix_commit>
```
