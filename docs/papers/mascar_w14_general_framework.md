# Mascar W14 General Paper Workload Framework Closeout

W14 closes out the common workload framework used by W9-W13.

## Components

- `experiments/common/gpgpusim_matrix/run_gpgpusim_matrix.sh`: CSV-driven matrix runner with per-run timeout and config selection.
- `experiments/common/gpgpusim_matrix/collect_gpgpusim_stats.py`: stats collector for GPGPU-Sim and Mascar M1-M4 counters, now including M3 probe diagnostics.
- `experiments/paper-mascar/workloads/matrix/W9_W14/prepare_w9_w14_manifests.py`: generates W9-W12 manifests while preserving Table III rows.
- `experiments/paper-mascar/workloads/matrix/W9_W14/analyze_mascar_stage.py`: stage-level activation/trend analyzer.

## Closeout Status

- W9 attempted M3 activation in 3 small rounds; no M3 strict/probe activation found.
- W10 expanded ready coverage to 15 rows and fixed config override/CSV command issues through direct command manifests.
- W11/W12 swept 15 ready rows across baseline/M2/M3/M4.
- W13 preserved all 30 Table III rows in the coverage manifest.
