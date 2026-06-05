# Branching Strategy

- `hrl/infra/paper-repro-framework-v0`: reusable framework from clean baseline; no simulator source modifications.
- `hrl/infra/paper-repro-framework-trace-v0`: optional extension branch derived from base infra; adds default-off kernel launch tracing.

Paper branches may cherry-pick reviewed infra fixes. Do not rebase paper branches onto infra branches.
