#!/usr/bin/env bash
set -euo pipefail

if [[ $# -ne 1 ]]; then
  echo "usage: $0 <output-directory>" >&2
  exit 2
fi

output_dir=$1
mkdir -p "$output_dir"
nvcc -arch=sm_52 -O0 -cudart shared "$(dirname "$0")/dtc_l1_legacy_hit.cu" \
  -o "$output_dir/dtc_l1_legacy_hit"
nvcc -arch=sm_52 -O0 -cudart shared "$(dirname "$0")/dtc_l1_legacy_merge.cu" \
  -o "$output_dir/dtc_l1_legacy_merge"
nvcc -arch=sm_52 -O0 -cudart shared "$(dirname "$0")/dtc_l1_legacy_bypass.cu" \
  -o "$output_dir/dtc_l1_legacy_bypass"
