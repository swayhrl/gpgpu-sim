#!/usr/bin/env bash
set -euo pipefail
if ! command -v nvcc >/dev/null 2>&1; then
  echo "nvcc unavailable" >&2
  exit 77
fi
nvcc -O2 -arch=sm_70 -cudart shared mascar_m3_diag.cu -o mascar_m3_diag
