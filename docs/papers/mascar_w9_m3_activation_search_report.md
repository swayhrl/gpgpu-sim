# Mascar W9 M3 Activation Input Search Report

W9 ran three small actual attempts over the W7 activation-ready candidate set: `spmv`, `mri_q`, and `pathfinder`. Each attempt preserved the 30-row Table III manifest while only the three candidate rows were actual-ready.

## Attempts

- attempt1: W7 large inputs, M3 and M4 configs.
- attempt2: W5C bounded inputs, M3 and M4 configs.
- attempt3: W7 large controls plus `pathfinder 512 512 4`, M3 and M4 configs.

## Activation

- M2 active workloads: 3
- M3 strict active workloads: 0
- M3 probe-observed workloads: 0
- M4 active workloads: 3
- strict active workloads: mri_q, pathfinder, spmv

No W9 attempt found strict M3 hit-only activation or M3 probe-observed counters under the current workload inputs. M2/M4 activation remains reproducible for the large-input attempts.
