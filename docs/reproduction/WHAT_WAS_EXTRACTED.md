# What Was Extracted

Extracted reusable paths:

- `experiments/common/gpgpusim_matrix/`: generic matrix runner, stats collector, and kernel trace parser.
- `experiments/common/README.md`: shared experiment tool documentation.
- `experiments/templates/`: CSV and report templates for future papers.
- `docs/reproduction/`: generic reproduction workflow documentation.
- `docs/reproduction_workload_framework.md`: workload framework overview.
- `tools/paper_repro/README.md`: placeholder/README for reusable paper reproduction tools.
- `experiments/suites/common/`: suite runner/collector/schema/taxonomy framework.
- `experiments/suites/README.md`: suite framework overview.

Local suite manifests, where present, were renamed to `*.local.example.csv` to make their machine/local-data nature explicit.

The stats collector may include optional parser fields for paper-specific counters such as Mascar counters. These are passive parsing fields only; this branch does not implement Mascar mechanisms.

## Local artifact cleanup

Round-specific W19/W21/W22 outputs and one-off recovery scripts were removed from the final base infra tree. Suite inventory/command manifests that are useful as examples were renamed to `*.local.example.csv`.
