#!/usr/bin/env bash
# stage_guard.sh — print 10-minute checkpoint reminder template
# Usage:
#   bash tools/paper_repro/stage_guard.sh --minutes 10 --note "running focused validation"
#
# This script does NOT control any process or tmux session.
# It only prints a reminder template for Claude to include in its prompt.

set -euo pipefail

MINUTES=10
NOTE=""

while [[ $# -gt 0 ]]; do
  case "$1" in
    --minutes) MINUTES="$2"; shift 2 ;;
    --note)    NOTE="$2";    shift 2 ;;
    *) echo "Unknown argument: $1" >&2; exit 1 ;;
  esac
done

cat <<EOF
## 10-Minute Checkpoint Rule

当前任务：${NOTE:-（未指定）}
时间限制：${MINUTES} 分钟

如果执行超过 ${MINUTES} 分钟，请立即暂停并输出以下格式的 checkpoint summary：

---
## Checkpoint Summary（约 ${MINUTES} 分钟）
- 已完成：
  - [ 列出已完成的步骤 ]
- 未完成：
  - [ 列出未完成的步骤 ]
- 当前状态：
  - [ 简述当前进度 ]
- 下一步：
  - [ 下一步操作 ]
---

输出 checkpoint 后停止，不要继续扩展内容。
等待用户确认后再继续。
EOF
