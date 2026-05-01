#!/usr/bin/env bash
# run_queue.sh — L3-lite supervisor wrapper
# Usage: bash tools/paper_repro/run_queue.sh <job_queue.yaml> [--dry-run] [--job <id>]
set -euo pipefail

REPO_ROOT="$(git -C "$(dirname "$0")" rev-parse --show-toplevel)"
SUPERVISOR="$REPO_ROOT/tools/paper_repro/supervisor.py"
QUEUE="${1:-}"
shift || true

if [[ -z "$QUEUE" ]]; then
  echo "Usage: bash run_queue.sh <job_queue.yaml> [--dry-run] [--job <id>]"
  exit 1
fi

echo "=== L3-lite paper repro supervisor ==="
echo "Queue: $QUEUE"
echo "Args:  $*"
echo ""

python3 "$SUPERVISOR" --queue "$QUEUE" "$@"

echo ""
echo "Output directory: $REPO_ROOT/runs/paper_repro_queue/"
echo "To inspect results:"
echo "  ls $REPO_ROOT/runs/paper_repro_queue/"
echo "  cat $REPO_ROOT/runs/paper_repro_queue/<job_id>/gpt_review_packet.md"
