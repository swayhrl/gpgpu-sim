#!/usr/bin/env bash
set -euo pipefail
exec ./spmv -i /workspace/repos/gpgpu-sim_distribution/experiments/paper-mascar/workloads/matrix/W5C/data/spmv_1024_d16.mtx,/workspace/repos/gpgpu-sim_distribution/experiments/paper-mascar/workloads/matrix/W5C/data/spmv_vec_1024.bin
