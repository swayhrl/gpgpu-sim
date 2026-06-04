# Mascar W3 Matrix Runner Framework

## Goal

W3 adds a reusable GPGPU-Sim matrix runner and stats collector for config x workload smoke testing. The framework is intentionally generic so future paper reproductions can reuse it outside Mascar.

## Common Runner

`experiments/common/gpgpusim_matrix/run_gpgpusim_matrix.sh` reads:

- `CONFIG_MATRIX`
- `WORKLOAD_MANIFEST`

It creates a timestamped or caller-provided output directory containing:

- `run_manifest.csv`
- `runs/<config_id>/<paper_id>/command.txt`
- `runs/<config_id>/<paper_id>/env.txt`
- `runs/<config_id>/<paper_id>/stdout.log`
- `runs/<config_id>/<paper_id>/stderr.log`
- `runs/<config_id>/<paper_id>/combined.log`
- `runs/<config_id>/<paper_id>/exit_code.txt`

The runner does not interpret benchmark correctness. It records preliminary status only from wrapper exit code and timeout state.

## Common Collector

`experiments/common/gpgpusim_matrix/collect_gpgpusim_stats.py` reads a run output directory and writes:

- `results.csv`
- `summary.md`
- `status_matrix.csv`

The collector scans runner logs and wrapper-owned logs under each run directory. This is required because W2 wrappers write simulator output to `MASCAR_RUN_DIR/<paper_id>.log`.

## Classification Policy

The collector distinguishes:

- `completed_explicit_pass`
- `completed_stats_found`
- `completed_no_explicit_pass`
- `completed_nonzero_with_stats`
- `phase_unknown`
- `missing_binary`
- `missing_source`
- `missing_data`
- `wrapper_unavailable`
- `timeout`
- `crash_assert`
- `simulator_error_no_stats`
- `no_stats_exit0`
- `unknown`

`completed_no_explicit_pass` is not treated as correctness pass.

## Mascar Table III Entry Points

Mascar W4 uses:

- `experiments/paper-mascar/workloads/matrix/run_mascar_table_iii_matrix.sh`
- `experiments/paper-mascar/workloads/matrix/collect_mascar_table_iii_matrix.py`
- `experiments/paper-mascar/workloads/matrix/mascar_w4_smoke_config_matrix.csv`

The W4 config matrix runs:

- `baseline_off`
- `m4_reexec_load`

The workload manifest remains the W1/W2 30-row Table III command manifest. `rodinia_hotspot` is not included.

## Validation

W3 validation performed:

- shell syntax check for the common runner.
- Python compile check for the common collector.
- shell syntax check for the Mascar runner.
- Python compile check for the Mascar collector wrapper.
- dry-run matrix over 30 workloads x 2 configs.
- collector pass over the dry-run output.

The dry-run matrix produced 60 rows and no actual simulator runs.
