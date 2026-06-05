#!/usr/bin/env bash
set -euo pipefail
script_dir="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
repo_root="$(cd "${script_dir}/../../../../.." && pwd)"
out_base="${OUT_BASE:-${repo_root}/experiments/paper-mascar/workloads/results/W15B}"
run_name="${RUN_NAME:-w15b_actual_$(date '+%Y%m%d_%H%M%S')}"
mkdir -p "${out_base}"
CONFIG_MATRIX="${script_dir}/w15b_config_matrix.csv" \
WORKLOAD_MANIFEST="${script_dir}/w15b_workload_manifest.csv" \
OUTDIR="${out_base}/${run_name}" \
TIMEOUT_SEC="${TIMEOUT_SEC:-900}" \
RUN_PLACEHOLDERS=0 \
RUN_READY=1 \
ONLY_READY=1 \
DRY_RUN_ONLY="${DRY_RUN_ONLY:-0}" \
SOURCE_ENV=1 \
"${repo_root}/experiments/common/gpgpusim_matrix/run_gpgpusim_matrix.sh"
