# Mascar W10 Table III Workload Expansion Report

W10 expanded local Table III coverage from 6 ready rows to 15 ready or approximate-ready rows. The approximate rows are phase-mapping approximations and remain labeled in notes.

## Debug

The first W10 actual run exposed a config propagation issue for `scripts/run_one.sh`-based commands. W10 manifests were revised to direct benchmark commands so the common wrapper can install and restore the selected matrix config in the benchmark CWD. A second issue came from comma-separated command arguments in CSV; `spmv` and `sgemm` were changed to `bash run_gpgpusim.sh` command wrappers to avoid comma splitting in the shell runner.

## Validation

- direct actual rows: 15
- rerun fixed rows: 2 (`spmv`, `sgemm`)
- ready rows for W11/W12: 15
- unavailable rows preserved: 15
