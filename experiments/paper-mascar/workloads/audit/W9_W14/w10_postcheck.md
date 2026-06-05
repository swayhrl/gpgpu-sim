# Mascar W10 Postcheck

- start_ts: 1780597289
- end_ts: 1780601902
- elapsed_sec: 4613
- branch: hrl/paper/mascar-repro-v0
- HEAD: e04bd787e35c082741adb7bc6267947c10e83d3d

## Summary

ready_rows=15; direct_actual_rows=15; rerun_fixed_rows=2

## Warnings

W10 uses approximate phase mappings for BP/histo/kmeans/SRAD split rows; paper phase equivalence remains approximate.

## Git Status

```text
 M experiments/common/gpgpusim_matrix/collect_gpgpusim_stats.py
?? experiments/paper-mascar/workloads/matrix/W10/
?? experiments/paper-mascar/workloads/matrix/W11/
?? experiments/paper-mascar/workloads/matrix/W12/
?? experiments/paper-mascar/workloads/matrix/W9/
?? experiments/paper-mascar/workloads/matrix/W9_W14/
?? experiments/paper-mascar/workloads/results/W10/
?? experiments/paper-mascar/workloads/results/W11/
?? experiments/paper-mascar/workloads/results/W12/
?? experiments/paper-mascar/workloads/results/W9/

```

## Validation

- `git diff --check`: pass
- `python3 -m py_compile` framework scripts: pass
- `bash -n` matrix/common wrapper scripts: pass
- key CSV row-count checks: pass
