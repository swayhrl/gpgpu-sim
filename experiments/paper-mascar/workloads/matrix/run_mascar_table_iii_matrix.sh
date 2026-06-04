#!/usr/bin/env bash
set -euo pipefail

script_dir="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
repo_root="$(cd "${script_dir}/../../../.." && pwd)"

CONFIG_MATRIX="${CONFIG_MATRIX:-${script_dir}/mascar_w4_smoke_config_matrix.csv}"
WORKLOAD_MANIFEST="${WORKLOAD_MANIFEST:-${repo_root}/experiments/paper-mascar/workloads/mascar_table_iii_command_manifest.csv}"
OUTDIR="${OUTDIR:-${repo_root}/experiments/paper-mascar/workloads/results/w4_smoke_$(date '+%Y%m%d_%H%M%S')}"
TIMEOUT_SEC="${TIMEOUT_SEC:-1200}"
RUN_PLACEHOLDERS="${RUN_PLACEHOLDERS:-1}"
RUN_READY="${RUN_READY:-1}"
DRY_RUN_ONLY="${DRY_RUN_ONLY:-0}"
SOURCE_ENV="${SOURCE_ENV:-1}"
GPGPUSIM_ROOT="${GPGPUSIM_ROOT:-${repo_root}}"
GPGPU_WORKLOAD_ROOT="${GPGPU_WORKLOAD_ROOT:-/workspace/repos/gpgpu-workloads}"

export CONFIG_MATRIX WORKLOAD_MANIFEST OUTDIR TIMEOUT_SEC RUN_PLACEHOLDERS RUN_READY
export DRY_RUN_ONLY SOURCE_ENV GPGPUSIM_ROOT GPGPU_WORKLOAD_ROOT

bash "${repo_root}/experiments/common/gpgpusim_matrix/run_gpgpusim_matrix.sh"
