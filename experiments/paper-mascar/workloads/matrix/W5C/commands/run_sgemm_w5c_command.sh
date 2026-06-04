#!/usr/bin/env bash
set -euo pipefail
exec ./sgemm -i /workspace/repos/gpgpu-sim_distribution/experiments/paper-mascar/workloads/matrix/W5C/data/sgemm_A_128x64.txt,/workspace/repos/gpgpu-sim_distribution/experiments/paper-mascar/workloads/matrix/W5C/data/sgemm_B_64x128.txt,/workspace/repos/gpgpu-sim_distribution/experiments/paper-mascar/workloads/matrix/W5C/data/sgemm_BT_128x64.txt
