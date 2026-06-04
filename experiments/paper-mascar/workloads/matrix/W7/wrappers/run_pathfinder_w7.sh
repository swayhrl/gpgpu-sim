#!/usr/bin/env bash
set -euo pipefail
repo_root="$(cd "$(dirname "${BASH_SOURCE[0]}")/../../../../../.." && pwd)"
export COMMAND_MANIFEST="${W7_COMMAND_MANIFEST:-${COMMAND_MANIFEST:-${repo_root}/experiments/paper-mascar/workloads/matrix/W7/w7_iter1_command_manifest.csv}}"
exec "${repo_root}/experiments/paper-mascar/workloads/wrappers/run_pathfinder.sh" "$@"
