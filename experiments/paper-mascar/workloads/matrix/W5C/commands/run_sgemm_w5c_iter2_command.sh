#!/usr/bin/env bash
set -euo pipefail
exec ./sgemm -i /workspace/repos/gpgpu-sim_distribution/experiments/paper-mascar/workloads/matrix/W5C/data/sgemm_A_128x32.txt,/workspace/repos/gpgpu-sim_distribution/experiments/paper-mascar/workloads/matrix/W5C/data/sgemm_B_32x64.txt,/workspace/repos/gpgpu-sim_distribution/experiments/paper-mascar/workloads/matrix/W5C/data/sgemm_BT_64x32.txt
