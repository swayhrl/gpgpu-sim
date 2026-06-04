#!/usr/bin/env bash
set -euo pipefail
repo_root="$(cd "$(dirname "${BASH_SOURCE[0]}")/../../../../../.." && pwd)"
export COMMAND_MANIFEST="${W5C_COMMAND_MANIFEST:-${COMMAND_MANIFEST:-${repo_root}/experiments/paper-mascar/workloads/matrix/W5C/w5c_command_manifest.csv}}"
exec "${repo_root}/experiments/paper-mascar/workloads/wrappers/run_sgemm.sh" "$@"
