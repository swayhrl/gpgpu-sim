# Mascar W25F Energy Rerun Closeout

W25F reran the focused W25 selected subset after W25E power field recovery. Scope was 6 workloads x 2 energy configs, not a full benchmark suite.

Actual run status: rows=12, completed=12, timeout=0, crash=0.

Power recovery: kernel_avg_power rows=12/12, gpu_tot_avg_power rows=12/12.

Derived energy method: `gpu_tot_avg_power * gpu_tot_sim_cycle / 1132e6`, using the QV100 config core clock. This is a current-simulator trend only and not a paper GPUWattch/GTX480 12% energy-saving reproduction.

All-workload mean GPU total power ratio: 1. All-workload geomean derived energy ratio: 1.00044.

Raw run directories were archived to `/workspace/tmp/mascar_w25f_raw_runs_20260605_223147.tar.gz` and removed from the repo tree.
