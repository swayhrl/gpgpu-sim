# Reproduction Workload Framework

This framework standardizes workload manifests, command manifests, config matrices, runners, collectors, status taxonomy, postchecks, and review packs for paper reproduction work. It preserves unavailable rows instead of dropping them and separates execution completion from correctness.

Core paths:

- `experiments/common/` for shared runners and collectors.
- `experiments/suites/common/` for Rodinia/Parboil suite manifests and smoke infrastructure.
- `experiments/templates/` for reusable CSV and report templates.
- `docs/reproduction/` for workflow documentation.
