# Git 工作流状态记录

记录时间：2026-04-29

## Remote 配置

| remote | URL |
|--------|-----|
| `origin` | `git@github.com:swayhrl/gpgpu-sim.git` |
| `upstream` | `https://github.com/gpgpu-sim/gpgpu-sim_distribution.git` |

## 已 push 分支

| 分支 | 用途 |
|------|------|
| `dev` | 跟踪上游官方仓库，不做实验改动 |
| `hrl/project-notes` | Day1-Day4 阅读笔记、CLAUDE.md、.claude/commands/ |
| `hrl/tlb-latency-v0` | **当前实验分支**，TLB latency model 实验代码 |

## Baseline Tag

- `baseline-a4ce3fe`：已创建并 push，对应上游 commit `a4ce3fe`

## 当前实验分支

**`hrl/tlb-latency-v0`**

用途：后续 TLB latency model 实验代码（`simple_tlb_t`、`tlb_latency_queue`、config 参数、统计项）。

## 代码修改前必须执行

```bash
git branch --show-current   # 期望输出：hrl/tlb-latency-v0
git status --short          # 期望输出：空（工作树干净）
```

## 注意事项

- **不要**重新设置 remote
- **不要**重新创建 baseline tag
- **不要**重新生成 SSH key
- `/save-session` 当前不可用：`save-session.md` command 文件存在于 `hrl/project-notes` 分支，
  未合并到 `hrl/tlb-latency-v0`。如需恢复，执行：
  ```bash
  git checkout hrl/project-notes -- .claude/commands/save-session.md
  ```
