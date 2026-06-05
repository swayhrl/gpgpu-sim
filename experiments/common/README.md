# Common Experiment Utilities

This directory contains reusable experiment helpers.

## gpgpusim_matrix

- `run_gpgpusim_matrix.sh` runs a config/workload CSV matrix with per-run timeouts.
- `collect_gpgpusim_stats.py` collects GPGPU-Sim stats plus Mascar M1-M4 counters.

The matrix runner expects CSV fields without unescaped commas because it is shell-based. For benchmark commands that require comma-separated arguments, wrap the command in a small script or call an existing `run_gpgpusim.sh` from the benchmark directory.
