#!/usr/bin/env bash
# check_repo_clean.sh — verify branch, clean status, and optional tag
# Usage:
#   bash tools/paper_repro/check_repo_clean.sh <expected_branch> [expected_tag]
# Exit 0 = all checks pass; non-zero = failure

set -euo pipefail

EXPECTED_BRANCH="${1:-}"
EXPECTED_TAG="${2:-}"

if [ -z "$EXPECTED_BRANCH" ]; then
  echo "ERROR: expected_branch argument required" >&2
  echo "Usage: $0 <expected_branch> [expected_tag]" >&2
  exit 1
fi

FAIL=0

# Check current branch
CURRENT_BRANCH=$(git branch --show-current 2>/dev/null)
if [ "$CURRENT_BRANCH" != "$EXPECTED_BRANCH" ]; then
  echo "FAIL: branch is '$CURRENT_BRANCH', expected '$EXPECTED_BRANCH'" >&2
  FAIL=1
else
  echo "OK: branch = $CURRENT_BRANCH"
fi

# Check working tree clean
STATUS=$(git status --short 2>/dev/null)
if [ -n "$STATUS" ]; then
  echo "FAIL: working tree is not clean:" >&2
  echo "$STATUS" >&2
  FAIL=1
else
  echo "OK: working tree clean"
fi

# Check tag exists (optional)
if [ -n "$EXPECTED_TAG" ]; then
  if git tag | grep -qx "$EXPECTED_TAG"; then
    echo "OK: tag '$EXPECTED_TAG' exists"
  else
    echo "FAIL: tag '$EXPECTED_TAG' not found" >&2
    FAIL=1
  fi
fi

if [ "$FAIL" -ne 0 ]; then
  echo "Pre-flight check FAILED. Stopping." >&2
  exit 1
fi

echo "Pre-flight check PASSED."
