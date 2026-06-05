# Mascar W25F Energy Rerun Execution

## Goal

Rerun the W25 selected 6 workloads x 2 energy configs using W25E runner/collector recovery.

## Commands run

- build check: `source setup_environment release && make -j2`
- dry-run: `W25F_DRY_RUN_ONLY=1 bash experiments/paper-mascar/energy/W25F/matrix/run_w25f_energy_rerun.sh`
- actual: `bash experiments/paper-mascar/energy/W25F/matrix/run_w25f_energy_rerun.sh`
- collector: `python3 experiments/common/gpgpusim_matrix/collect_gpgpusim_stats.py <outdir>`

## Dry-run status

Dry-run rows: 12.

## Actual run status

Actual rows: 12; completed=12; timeout=0; crash=0.

## Artifact recovery status

Rows with `power_artifacts/`: 12/12.

## Power field recovery status

Rows with `kernel_avg_power`: 12/12. Rows with `gpu_tot_avg_power`: 12/12.

## Failures and fixes

No additional runner or collector fix was required during W25F; W25E fixes were sufficient.

## Raw log archive note

Raw run directories are archived in the W25F closeout under `/workspace/tmp` and stable CSV/markdown summaries are retained in the repo tree.
